// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "third_party/libXNVCtrl/NVCtrl.h"
#include "third_party/libXNVCtrl/NVCtrlLib.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"

namespace {

// PciDevice and PciAccess are defined to access libpci functions.  Their
// members match the corresponding structures defined by libpci in size up to
// fields we may access.  For those members we don't use, their names are
// defined as "fieldX", etc., or, left out if they are declared after the
// members we care about in libpci.

struct PciDevice {
  PciDevice* next;

  uint16 field0;
  uint8 field1;
  uint8 field2;
  uint8 field3;
  int field4;

  uint16 vendor_id;
  uint16 device_id;
  uint16 device_class;
};

struct PciAccess {
  unsigned int field0;
  int field1;
  int field2;
  char* field3;
  int field4;
  int field5;
  unsigned int field6;
  int field7;

  void (*function0)();
  void (*function1)();
  void (*function2)();

  PciDevice* device_list;
};

// Define function types.
typedef PciAccess* (*FT_pci_alloc)();
typedef void (*FT_pci_init)(PciAccess*);
typedef void (*FT_pci_cleanup)(PciAccess*);
typedef void (*FT_pci_scan_bus)(PciAccess*);
typedef void (*FT_pci_scan_bus)(PciAccess*);
typedef int (*FT_pci_fill_info)(PciDevice*, int);
typedef char* (*FT_pci_lookup_name)(PciAccess*, char*, int, int, ...);

// This includes dynamically linked library handle and functions pointers from
// libpci.
struct PciInterface {
  void* lib_handle;

  FT_pci_alloc pci_alloc;
  FT_pci_init pci_init;
  FT_pci_cleanup pci_cleanup;
  FT_pci_scan_bus pci_scan_bus;
  FT_pci_fill_info pci_fill_info;
  FT_pci_lookup_name pci_lookup_name;
};

// This checks if a system supports PCI bus.
// We check the existence of /sys/bus/pci or /sys/bug/pci_express.
bool IsPciSupported() {
  const FilePath pci_path("/sys/bus/pci/");
  const FilePath pcie_path("/sys/bus/pci_express/");
  return (file_util::PathExists(pci_path) ||
          file_util::PathExists(pcie_path));
}

// This dynamically opens libpci and get function pointers we need.  Return
// NULL if library fails to open or any functions can not be located.
// Returned interface (if not NULL) should be deleted in FinalizeLibPci.
PciInterface* InitializeLibPci(const char* lib_name) {
  void* handle = dlopen(lib_name, RTLD_LAZY);
  if (handle == NULL) {
    VLOG(1) << "Failed to dlopen " << lib_name;
    return NULL;
  }
  PciInterface* interface = new struct PciInterface;
  interface->lib_handle = handle;
  interface->pci_alloc = reinterpret_cast<FT_pci_alloc>(
      dlsym(handle, "pci_alloc"));
  interface->pci_init = reinterpret_cast<FT_pci_init>(
      dlsym(handle, "pci_init"));
  interface->pci_cleanup = reinterpret_cast<FT_pci_cleanup>(
      dlsym(handle, "pci_cleanup"));
  interface->pci_scan_bus = reinterpret_cast<FT_pci_scan_bus>(
      dlsym(handle, "pci_scan_bus"));
  interface->pci_fill_info = reinterpret_cast<FT_pci_fill_info>(
      dlsym(handle, "pci_fill_info"));
  interface->pci_lookup_name = reinterpret_cast<FT_pci_lookup_name>(
      dlsym(handle, "pci_lookup_name"));
  if (interface->pci_alloc == NULL ||
      interface->pci_init == NULL ||
      interface->pci_cleanup == NULL ||
      interface->pci_scan_bus == NULL ||
      interface->pci_fill_info == NULL ||
      interface->pci_lookup_name == NULL) {
    VLOG(1) << "Missing required function(s) from " << lib_name;
    dlclose(handle);
    delete interface;
    return NULL;
  }
  return interface;
}

// This close the dynamically opened libpci and delete the interface.
void FinalizeLibPci(PciInterface** interface) {
  DCHECK(interface && *interface && (*interface)->lib_handle);
  dlclose((*interface)->lib_handle);
  delete (*interface);
  *interface = NULL;
}

// Scan /etc/ati/amdpcsdb.default for "ReleaseVersion".
// Return empty string on failing.
std::string CollectDriverVersionATI() {
  const FilePath::CharType kATIFileName[] =
      FILE_PATH_LITERAL("/etc/ati/amdpcsdb.default");
  FilePath ati_file_path(kATIFileName);
  if (!file_util::PathExists(ati_file_path))
    return std::string();
  std::string contents;
  if (!file_util::ReadFileToString(ati_file_path, &contents))
    return std::string();
  StringTokenizer t(contents, "\r\n");
  while (t.GetNext()) {
    std::string line = t.token();
    if (StartsWithASCII(line, "ReleaseVersion=", true)) {
      size_t begin = line.find_first_of("0123456789");
      if (begin != std::string::npos) {
        size_t end = line.find_first_not_of("0123456789.", begin);
        if (end == std::string::npos)
          return line.substr(begin);
        else
          return line.substr(begin, end - begin);
      }
    }
  }
  return std::string();
}

// Use NVCtrl extention to query NV driver version.
// Return empty string on failing.
std::string CollectDriverVersionNVidia() {
  Display* display = base::MessagePumpForUI::GetDefaultXDisplay();
  if (!display) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return std::string();
  }
  int event_base = 0, error_base = 0;
  if (!XNVCTRLQueryExtension(display, &event_base, &error_base)) {
    LOG(INFO) << "NVCtrl extension does not exist.";
    return std::string();
  }
  int screen_count = ScreenCount(display);
  for (int screen = 0; screen < screen_count; ++screen) {
    char* buffer = NULL;
    if (XNVCTRLIsNvScreen(display, screen) &&
        XNVCTRLQueryStringAttribute(display, screen, 0,
                                    NV_CTRL_STRING_NVIDIA_DRIVER_VERSION,
                                    &buffer)) {
      std::string driver_version(buffer);
      XFree(buffer);
      return driver_version;
    }
  }
  return std::string();
}

const uint32 kVendorIDIntel = 0x8086;
const uint32 kVendorIDNVidia = 0x10de;
const uint32 kVendorIDAMD = 0x1002;

}  // namespace anonymous

namespace gpu_info_collector {

bool CollectGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGpuNoContextLost)) {
    gpu_info->can_lose_context = false;
  } else {
    // TODO(zmo): need to consider the case where we are running on top
    // of desktop GL and GL_ARB_robustness extension is available.
    gpu_info->can_lose_context =
        (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2);
  }

  gpu_info->finalized = true;
  bool rt = CollectGraphicsInfoGL(gpu_info);

  return rt;
}

bool CollectPreliminaryGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  bool rt = CollectVideoCardInfo(gpu_info);

  std::string driver_version;
  switch (gpu_info->gpu.vendor_id) {
    case kVendorIDAMD:
      driver_version = CollectDriverVersionATI();
      if (!driver_version.empty()) {
        gpu_info->driver_vendor = "ATI / AMD";
        gpu_info->driver_version = driver_version;
      }
      break;
    case kVendorIDNVidia:
      driver_version = CollectDriverVersionNVidia();
      if (!driver_version.empty()) {
        gpu_info->driver_vendor = "NVIDIA";
        gpu_info->driver_version = driver_version;
      }
      break;
    case kVendorIDIntel:
      // In dual-GPU cases, sometimes PCI scan only gives us the
      // integrated GPU (i.e., the Intel one).
      driver_version = CollectDriverVersionNVidia();
      if (!driver_version.empty()) {
        gpu_info->driver_vendor = "NVIDIA";
        gpu_info->driver_version = driver_version;
        // Machines with more than two GPUs are not handled.
        if (gpu_info->secondary_gpus.size() <= 1)
          gpu_info->optimus = true;
      }
      break;
  }

  return rt;
}

bool CollectVideoCardInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  if (IsPciSupported() == false) {
    VLOG(1) << "PCI bus scanning is not supported";
    return false;
  }

  // TODO(zmo): be more flexible about library name.
  PciInterface* interface = InitializeLibPci("libpci.so.3");
  if (interface == NULL)
    interface = InitializeLibPci("libpci.so");
  if (interface == NULL) {
    VLOG(1) << "Failed to locate libpci";
    return false;
  }

  PciAccess* access = (interface->pci_alloc)();
  DCHECK(access != NULL);
  (interface->pci_init)(access);
  (interface->pci_scan_bus)(access);
  bool primary_gpu_identified = false;
  for (PciDevice* device = access->device_list;
       device != NULL; device = device->next) {
    (interface->pci_fill_info)(device, 33);  // Fill the IDs and class fields.
    // TODO(zmo): there might be other classes that qualify as display devices.
    if (device->device_class != 0x0300)  // Device class is DISPLAY_VGA.
      continue;

    content::GPUInfo::GPUDevice gpu;
    gpu.vendor_id = device->vendor_id;
    gpu.device_id = device->device_id;

    const int buffer_size = 255;
    scoped_array<char> buffer(new char[buffer_size]);
    // The current implementation of pci_lookup_name returns the same pointer
    // as the passed in upon success, and a different one (NULL or a pointer
    // to an error message) upon failure.
    if ((interface->pci_lookup_name)(access,
                                     buffer.get(),
                                     buffer_size,
                                     1,
                                     device->vendor_id) == buffer.get()) {
      gpu.vendor_string = buffer.get();
    }
    if ((interface->pci_lookup_name)(access,
                                     buffer.get(),
                                     buffer_size,
                                     2,
                                     device->vendor_id,
                                     device->device_id) == buffer.get()) {
      std::string device_string = buffer.get();
      size_t begin = device_string.find_first_of('[');
      size_t end = device_string.find_last_of(']');
      if (begin != std::string::npos && end != std::string::npos &&
          begin < end) {
        device_string = device_string.substr(begin + 1, end - begin - 1);
      }
      gpu.device_string = device_string;
    }

    if (!primary_gpu_identified) {
      primary_gpu_identified = true;
      gpu_info->gpu = gpu;
    } else {
      // TODO(zmo): if there are multiple GPUs, we assume the non Intel
      // one is primary. Revisit this logic because we actually don't know
      // which GPU we are using at this point.
      if (gpu_info->gpu.vendor_id == kVendorIDIntel &&
          gpu.vendor_id != kVendorIDIntel) {
        gpu_info->secondary_gpus.push_back(gpu_info->gpu);
        gpu_info->gpu = gpu;
      } else {
        gpu_info->secondary_gpus.push_back(gpu);
      }
    }
  }

  // Detect Optimus or AMD Switchable GPU.
  if (gpu_info->secondary_gpus.size() == 1 &&
      gpu_info->secondary_gpus[0].vendor_id == kVendorIDIntel) {
    if (gpu_info->gpu.vendor_id == kVendorIDNVidia)
      gpu_info->optimus = true;
    if (gpu_info->gpu.vendor_id == kVendorIDAMD)
      gpu_info->amd_switchable = true;
  }

  (interface->pci_cleanup)(access);
  FinalizeLibPci(&interface);
  return (primary_gpu_identified);
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  std::string gl_version_string = gpu_info->gl_version_string;
  if (StartsWithASCII(gl_version_string, "OpenGL ES", true))
    gl_version_string = gl_version_string.substr(10);
  std::vector<std::string> pieces;
  base::SplitStringAlongWhitespace(gl_version_string, &pieces);
  // In linux, the gl version string might be in the format of
  //   GLVersion DriverVendor DriverVersion
  if (pieces.size() < 3)
    return false;

  std::string driver_version = pieces[2];
  size_t pos = driver_version.find_first_not_of("0123456789.");
  if (pos == 0)
    return false;
  if (pos != std::string::npos)
    driver_version = driver_version.substr(0, pos);

  gpu_info->driver_vendor = pieces[1];
  gpu_info->driver_version = driver_version;
  return true;
}

}  // namespace gpu_info_collector
