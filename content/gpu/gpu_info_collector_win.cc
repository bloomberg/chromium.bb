// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

// This has to be included before windows.h.
#include "third_party/re2/re2/re2.h"

#include <windows.h>
#include <d3d9.h>
#include <setupapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/metrics/histogram.h"
#include "base/scoped_native_library.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"

namespace {

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
  TRACE_EVENT0("gpu", "RetrieveGpuPerformanceStats");

  // If the user re-runs the assessment without restarting, the COM API
  // returns WINSAT_ASSESSMENT_STATE_NOT_AVAILABLE. Because of that and
  // http://crbug.com/124325, read the assessment result files directly.
  content::GpuPerformanceStats stats;

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

  return stats;
}

content::GpuPerformanceStats RetrieveGpuPerformanceStatsWithHistograms() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  content::GpuPerformanceStats stats = RetrieveGpuPerformanceStats();

  UMA_HISTOGRAM_TIMES("GPU.WinSAT.ReadResultsFileTime",
                      base::TimeTicks::Now() - start_time);
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.OverallScore2",
                              stats.overall * 10, 10, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.GraphicsScore2",
                              stats.graphics * 10, 10, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.WinSAT.GamingScore2",
                              stats.gaming * 10, 10, 200, 50);
  UMA_HISTOGRAM_BOOLEAN(
      "GPU.WinSAT.HasResults",
      stats.overall != 0.0 && stats.graphics != 0.0 && stats.gaming != 0.0);

  return stats;
}

}  // namespace anonymous

namespace gpu_info_collector {

#if !defined(GOOGLE_CHROME_BUILD)
AMDVideoCardType GetAMDVideocardType() {
  return UNKNOWN;
}
#else
// This function has a real implementation for official builds that can
// be found in src/third_party/amd.
AMDVideoCardType GetAMDVideocardType();
#endif

bool CollectDriverInfoD3D(const std::wstring& device_id,
                          content::GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoD3D");

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
          if (driver_vendor == "Advanced Micro Devices, Inc." ||
              driver_vendor == "ATI Technologies Inc.") {
            // We are conservative and assume that in the absence of a clear
            // signal the videocard is assumed to be switchable. Additionally,
            // some switchable systems with Intel GPUs aren't correctly
            // detected, so always count them.
            AMDVideoCardType amd_card_type = GetAMDVideocardType();
            gpu_info->amd_switchable = (gpu_info->gpu.vendor_id == 0x8086) ||
                                       (amd_card_type != STANDALONE);
          }
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

bool CollectContextGraphicsInfo(content::GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectGraphicsInfo");

  DCHECK(gpu_info);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL)) {
    std::string requested_implementation_name =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kUseGL);
    if (requested_implementation_name == "swiftshader") {
      gpu_info->software_rendering = true;
      return false;
    }
  }

  if (!CollectGraphicsInfoGL(gpu_info))
    return false;

  // ANGLE's renderer strings are of the form:
  // ANGLE (<adapter_identifier> Direct3D<version> vs_x_x ps_x_x)
  std::string direct3d_version;
  int vertex_shader_major_version = 0;
  int vertex_shader_minor_version = 0;
  int pixel_shader_major_version = 0;
  int pixel_shader_minor_version = 0;
  if (RE2::FullMatch(gpu_info->gl_renderer,
                     "ANGLE \\(.*\\)") &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " Direct3D(\\w+)",
                        &direct3d_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " vs_(\\d+)_(\\d+)",
                        &vertex_shader_major_version,
                        &vertex_shader_minor_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " ps_(\\d+)_(\\d+)",
                        &pixel_shader_major_version,
                        &pixel_shader_minor_version)) {
    gpu_info->can_lose_context = direct3d_version == "9";
    gpu_info->vertex_shader_version =
        StringPrintf("%d.%d",
                     vertex_shader_major_version,
                     vertex_shader_minor_version);
    gpu_info->pixel_shader_version =
        StringPrintf("%d.%d",
                     pixel_shader_major_version,
                     pixel_shader_minor_version);

    // DirectX diagnostics are collected asynchronously because it takes a
    // couple of seconds. Do not mark gpu_info as complete until that is done.
    gpu_info->finalized = false;
  } else {
    gpu_info->finalized = true;
  }

  return true;
}

bool CollectBasicGraphicsInfo(content::GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectPreliminaryGraphicsInfo");

  DCHECK(gpu_info);

  gpu_info->performance_stats = RetrieveGpuPerformanceStatsWithHistograms();

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

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoGL");

  if (!gpu_info->driver_version.empty())
    return true;

  std::string gl_version_string = gpu_info->gl_version_string;

  return RE2::PartialMatch(gl_version_string,
                           "([\\d\\.]+)$",
                           &gpu_info->driver_version);
}

void MergeGPUInfo(content::GPUInfo* basic_gpu_info,
                  const content::GPUInfo& context_gpu_info) {
  DCHECK(basic_gpu_info);

  if (context_gpu_info.software_rendering) {
    basic_gpu_info->software_rendering = true;
    return;
  }

  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);

  basic_gpu_info->dx_diagnostics = context_gpu_info.dx_diagnostics;
}

}  // namespace gpu_info_collector
