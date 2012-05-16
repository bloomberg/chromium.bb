// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include <windows.h>
#include <d3d9.h>
#include <setupapi.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/scoped_native_library.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"

// ANGLE seems to require that main.h be included before any other ANGLE header.
#include "libEGL/Display.h"
#include "libEGL/main.h"

namespace {

// The version number stores the major and minor version in the least 16 bits;
// for example, 2.5 is 0x00000205.
// Returned string is in the format of "major.minor".
std::string VersionNumberToString(uint32 version_number) {
  int hi = (version_number >> 8) & 0xff;
  int low = version_number & 0xff;
  return base::IntToString(hi) + "." + base::IntToString(low);
}

float ReadXMLFloatValue(XmlReader* reader) {
  std::string score_string;
  if (!reader->ReadElementContent(&score_string))
    return 0.0;

  double score;
  if (!base::StringToDouble(score_string, &score))
    return 0.0;

  return static_cast<float>(score);
}

content::GpuPerformanceStats RetrieveGpuPerformanceStats() {
  // If the user re-runs the assessment without restarting, the COM API
  // returns WINSAT_ASSESSMENT_STATE_NOT_AVAILABLE. Because of that and
  // http://crbug.com/124325, read the assessment result files directly.
  content::GpuPerformanceStats stats;

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Get path to WinSAT results files.
  wchar_t winsat_results_path[MAX_PATH];
  DWORD size = ExpandEnvironmentStrings(
      L"%WinDir%\\Performance\\WinSAT\\DataStore\\",
      winsat_results_path, MAX_PATH);
  if (size == 0 || size > MAX_PATH) {
    LOG(ERROR) << "The path to the WinSAT results is too long: "
               << size << " chars.";
    return stats;
  }

  // Find most recent formal assessment results.
  file_util::FileEnumerator file_enumerator(
      FilePath(winsat_results_path),
      false,  // not recursive
      file_util::FileEnumerator::FILES,
      FILE_PATH_LITERAL("* * Formal.Assessment (*).WinSAT.xml"));

  FilePath current_results;
  for (FilePath results = file_enumerator.Next(); !results.empty();
       results = file_enumerator.Next()) {
    // The filenames start with the date and time as yyyy-mm-dd hh.mm.ss.xxx,
    // so the greatest file lexicographically is also the most recent file.
    if (FilePath::CompareLessIgnoreCase(current_results.value(),
                                        results.value()))
      current_results = results;
  }

  std::string current_results_string = current_results.MaybeAsASCII();
  if (current_results_string.empty()) {
    LOG(ERROR) << "Can't retrieve a valid WinSAT assessment.";
    return stats;
  }

  // Get relevant scores from results file. XML schema at:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa969210.aspx
  XmlReader reader;
  if (!reader.LoadFile(current_results_string)) {
    LOG(ERROR) << "Could not open WinSAT results file.";
    return stats;
  }
  // Descend into <WinSAT> root element.
  if (!reader.SkipToElement() || !reader.Read()) {
    LOG(ERROR) << "Could not read WinSAT results file.";
    return stats;
  }

  // Search for <WinSPR> element containing the results.
  do {
    if (reader.NodeName() == "WinSPR")
      break;
  } while (reader.Next());
  // Descend into <WinSPR> element.
  if (!reader.Read()) {
    LOG(ERROR) << "Could not find WinSPR element in results file.";
    return stats;
  }

  // Read scores.
  for (int depth = reader.Depth(); reader.Depth() == depth; reader.Next()) {
    std::string node_name = reader.NodeName();
    if (node_name == "SystemScore")
      stats.overall = ReadXMLFloatValue(&reader);
    else if (node_name == "GraphicsScore")
      stats.graphics = ReadXMLFloatValue(&reader);
    else if (node_name == "GamingScore")
      stats.gaming = ReadXMLFloatValue(&reader);
  }

  if (stats.overall == 0.0)
    LOG(ERROR) << "Could not read overall score from assessment results.";
  if (stats.graphics == 0.0)
    LOG(ERROR) << "Could not read graphics score from assessment results.";
  if (stats.gaming == 0.0)
    LOG(ERROR) << "Could not read gaming score from assessment results.";

  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.OverallScore",
                              stats.overall, 0.0, 50.0, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.GraphicsScore",
                              stats.graphics, 0.0, 50.0, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.GamingScore",
                              stats.gaming, 0.0, 50.0, 50);

  UMA_HISTOGRAM_TIMES("GPU.WinSAT.ReadResultsFileTime",
                      base::TimeTicks::Now() - start_time);

  return stats;
}

}  // namespace anonymous

namespace gpu_info_collector {

bool CollectGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  content::GpuPerformanceStats stats = RetrieveGpuPerformanceStats();
  UMA_HISTOGRAM_BOOLEAN(
      "GPU.WinSAT.HasResults",
      stats.overall != 0.0 && stats.graphics != 0.0 && stats.gaming != 0.0);
  gpu_info->performance_stats = stats;

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL)) {
    std::string requested_implementation_name =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kUseGL);
    if (requested_implementation_name == "swiftshader") {
      gpu_info->software_rendering = true;
      return false;
    }
  }

  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    gpu_info->finalized = true;
    return CollectGraphicsInfoGL(gpu_info);
  }

  // TODO(zmo): the following code only works if running on top of ANGLE.
  // Need to handle the case when running on top of real EGL/GLES2 drivers.

  egl::Display* display = static_cast<egl::Display*>(
      gfx::GLSurfaceEGL::GetHardwareDisplay());
  if (!display) {
    LOG(ERROR) << "gfx::BaseEGLContext::GetDisplay() failed";
    return false;
  }

  IDirect3DDevice9* device = display->getDevice();
  if (!device) {
    LOG(ERROR) << "display->getDevice() failed";
    return false;
  }

  base::win::ScopedComPtr<IDirect3D9> d3d;
  if (FAILED(device->GetDirect3D(d3d.Receive()))) {
    LOG(ERROR) << "device->GetDirect3D(&d3d) failed";
    return false;
  }

  if (!CollectGraphicsInfoD3D(d3d, gpu_info))
    return false;

  // DirectX diagnostics are collected asynchronously because it takes a
  // couple of seconds. Do not mark gpu_info as complete until that is done.
  return true;
}

bool CollectPreliminaryGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  bool rt = true;
  if (!CollectVideoCardInfo(gpu_info))
    rt = false;

  gpu_info->performance_stats = RetrieveGpuPerformanceStats();

  return rt;
}

bool CollectGraphicsInfoD3D(IDirect3D9* d3d, content::GPUInfo* gpu_info) {
  DCHECK(d3d);
  DCHECK(gpu_info);

  bool succeed = CollectVideoCardInfo(gpu_info);

  // Get version information
  D3DCAPS9 d3d_caps;
  if (d3d->GetDeviceCaps(D3DADAPTER_DEFAULT,
                         D3DDEVTYPE_HAL,
                         &d3d_caps) == D3D_OK) {
    gpu_info->pixel_shader_version =
        VersionNumberToString(d3d_caps.PixelShaderVersion);
    gpu_info->vertex_shader_version =
        VersionNumberToString(d3d_caps.VertexShaderVersion);
  } else {
    LOG(ERROR) << "d3d->GetDeviceCaps() failed";
    succeed = false;
  }

  // Get can_lose_context
  base::win::ScopedComPtr<IDirect3D9Ex> d3dex;
  if (SUCCEEDED(d3dex.QueryFrom(d3d)))
    gpu_info->can_lose_context = false;
  else
    gpu_info->can_lose_context = true;

  return true;
}

bool CollectVideoCardInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // nvd3d9wrap.dll is loaded into all processes when Optimus is enabled.
  HMODULE nvd3d9wrap = GetModuleHandleW(L"nvd3d9wrap.dll");
  gpu_info->optimus = nvd3d9wrap != NULL;

  // Taken from http://developer.nvidia.com/object/device_ids.html
  DISPLAY_DEVICE dd;
  dd.cb = sizeof(DISPLAY_DEVICE);
  int i = 0;
  std::wstring id;
  for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i) {
    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      id = dd.DeviceID;
      break;
    }
  }

  if (id.length() > 20) {
    int vendor_id = 0, device_id = 0;
    std::wstring vendor_id_string = id.substr(8, 4);
    std::wstring device_id_string = id.substr(17, 4);
    base::HexStringToInt(WideToASCII(vendor_id_string), &vendor_id);
    base::HexStringToInt(WideToASCII(device_id_string), &device_id);
    gpu_info->gpu.vendor_id = vendor_id;
    gpu_info->gpu.device_id = device_id;
    // TODO(zmo): we only need to call CollectDriverInfoD3D() if we use ANGLE.
    return CollectDriverInfoD3D(id, gpu_info);
  }
  return false;
}

bool CollectDriverInfoD3D(const std::wstring& device_id,
                          content::GPUInfo* gpu_info) {
  // create device info for the display device
  HDEVINFO device_info = SetupDiGetClassDevsW(
      NULL, device_id.c_str(), NULL,
      DIGCF_PRESENT | DIGCF_PROFILE | DIGCF_ALLCLASSES);
  if (device_info == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Creating device info failed";
    return false;
  }

  DWORD index = 0;
  bool found = false;
  SP_DEVINFO_DATA device_info_data;
  device_info_data.cbSize = sizeof(device_info_data);
  while (SetupDiEnumDeviceInfo(device_info, index++, &device_info_data)) {
    WCHAR value[255];
    if (SetupDiGetDeviceRegistryPropertyW(device_info,
                                        &device_info_data,
                                        SPDRP_DRIVER,
                                        NULL,
                                        reinterpret_cast<PBYTE>(value),
                                        sizeof(value),
                                        NULL)) {
      HKEY key;
      std::wstring driver_key = L"System\\CurrentControlSet\\Control\\Class\\";
      driver_key += value;
      LONG result = RegOpenKeyExW(
          HKEY_LOCAL_MACHINE, driver_key.c_str(), 0, KEY_QUERY_VALUE, &key);
      if (result == ERROR_SUCCESS) {
        DWORD dwcb_data = sizeof(value);
        std::string driver_version;
        result = RegQueryValueExW(
            key, L"DriverVersion", NULL, NULL,
            reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          driver_version = WideToASCII(std::wstring(value));

        std::string driver_date;
        dwcb_data = sizeof(value);
        result = RegQueryValueExW(
            key, L"DriverDate", NULL, NULL,
            reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          driver_date = WideToASCII(std::wstring(value));

        std::string driver_vendor;
        dwcb_data = sizeof(value);
        result = RegQueryValueExW(
            key, L"ProviderName", NULL, NULL,
            reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS) {
          driver_vendor = WideToASCII(std::wstring(value));
          // If it's an Intel GPU with a driver provided by AMD then it's
          // probably AMD's Dynamic Switchable Graphics.
          // TODO: detect only AMD switchable
          gpu_info->amd_switchable =
              driver_vendor == "Advanced Micro Devices, Inc." ||
              driver_vendor == "ATI Technologies Inc.";
        }

        gpu_info->driver_vendor = driver_vendor;
        gpu_info->driver_version = driver_version;
        gpu_info->driver_date = driver_date;
        found = true;
        RegCloseKey(key);
        break;
      }
    }
  }
  SetupDiDestroyDeviceInfoList(device_info);
  return found;
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  std::string gl_version_string = gpu_info->gl_version_string;

  // TODO(zmo): We assume the driver version is in the end of GL_VERSION
  // string.  Need to verify if it is true for majority drivers.

  size_t pos = gl_version_string.find_last_not_of("0123456789.");
  if (pos != std::string::npos && pos < gl_version_string.length() - 1) {
    gpu_info->driver_version = gl_version_string.substr(pos + 1);
    return true;
  }
  return false;
}

}  // namespace gpu_info_collector
