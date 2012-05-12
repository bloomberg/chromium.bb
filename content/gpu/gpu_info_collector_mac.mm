// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include <vector>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_interface.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>

namespace {

struct VideoCardInfo {
  UInt32 vendor_id;
  UInt32 device_id;

  VideoCardInfo(UInt32 vendor, UInt32 device) {
    vendor_id = vendor;
    device_id = device;
  }
};

CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                CFStringRef propertyName) {
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}

UInt32 IntValueOfCFData(CFDataRef data_ref) {
  DCHECK(data_ref);

  UInt32 value = 0;
  const UInt32* value_pointer =
      reinterpret_cast<const UInt32*>(CFDataGetBytePtr(data_ref));
  if (value_pointer != NULL)
    value = *value_pointer;
  return value;
}

// Scan IO registry for PCI video cards.
// If two cards are located, assume the non-Intel card is the high-end
// one that's going to be used by Chromium GPU process.
// If more than two cards are located, return false.  In such rare situation,
// video card information should be collected through identifying the currently
// in-use card as in CollectVideoCardInfo().
bool CollectPCIVideoCardInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // match_dictionary will be consumed by IOServiceGetMatchingServices, no need
  // to release it.
  CFMutableDictionaryRef match_dictionary = IOServiceMatching("IOPCIDevice");
  io_iterator_t entry_iterator;
  if (IOServiceGetMatchingServices(kIOMasterPortDefault,
                                   match_dictionary,
                                   &entry_iterator) != kIOReturnSuccess)
    return false;

  std::vector<VideoCardInfo> video_card_list;
  io_registry_entry_t entry;
  while ((entry = IOIteratorNext(entry_iterator))) {
    base::mac::ScopedCFTypeRef<CFDataRef> class_code_ref(static_cast<CFDataRef>(
        SearchPortForProperty(entry, CFSTR("class-code"))));
    if (!class_code_ref)
      continue;
    UInt32 class_code = IntValueOfCFData(class_code_ref);
    if (class_code != 0x30000)  // DISPLAY_VGA
      continue;
    base::mac::ScopedCFTypeRef<CFDataRef> vendor_id_ref(static_cast<CFDataRef>(
        SearchPortForProperty(entry, CFSTR("vendor-id"))));
    if (!vendor_id_ref)
      continue;
    UInt32 vendor_id = IntValueOfCFData(vendor_id_ref);
    base::mac::ScopedCFTypeRef<CFDataRef> device_id_ref(static_cast<CFDataRef>(
        SearchPortForProperty(entry, CFSTR("device-id"))));
    if (!device_id_ref)
      continue;
    UInt32 device_id = IntValueOfCFData(device_id_ref);
    video_card_list.push_back(VideoCardInfo(vendor_id, device_id));
  }
  IOObjectRelease(entry_iterator);

  const UInt32 kIntelVendorId = 0x8086;
  size_t found = video_card_list.size();
  switch (video_card_list.size()) {
    case 1:
      found = 0;
      break;
    case 2:
      if (video_card_list[0].vendor_id == kIntelVendorId &&
          video_card_list[1].vendor_id != kIntelVendorId)
        found = 1;
      else if (video_card_list[0].vendor_id != kIntelVendorId &&
               video_card_list[1].vendor_id == kIntelVendorId)
        found = 0;
      break;
  }
  if (found < video_card_list.size()) {
    gpu_info->gpu.vendor_id = video_card_list[found].vendor_id;
    gpu_info->gpu.device_id = video_card_list[found].device_id;
    return true;
  }
  return false;
}

}  // namespace anonymous

namespace gpu_info_collector {

bool CollectGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  gpu_info->can_lose_context =
      (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2);
  gpu_info->finalized = true;
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectPreliminaryGraphicsInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  bool rt = true;
  if (!CollectPCIVideoCardInfo(gpu_info) && !CollectVideoCardInfo(gpu_info))
    rt = false;

  return rt;
}

bool CollectVideoCardInfo(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  UInt32 vendor_id = 0, device_id = 0;
  io_registry_entry_t dsp_port = CGDisplayIOServicePort(kCGDirectMainDisplay);
  CFTypeRef vendor_id_ref = SearchPortForProperty(dsp_port, CFSTR("vendor-id"));
  if (vendor_id_ref) {
    vendor_id = IntValueOfCFData((CFDataRef)vendor_id_ref);
    CFRelease(vendor_id_ref);
  }
  CFTypeRef device_id_ref = SearchPortForProperty(dsp_port, CFSTR("device-id"));
  if (device_id_ref) {
    device_id = IntValueOfCFData((CFDataRef)device_id_ref);
    CFRelease(device_id_ref);
  }

  gpu_info->gpu.vendor_id = vendor_id;
  gpu_info->gpu.device_id = device_id;
  return true;
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // Extract the OpenGL driver version string from the GL_VERSION string.
  // Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  // Use some jiggery-pokery to turn that utf8 string into a std::wstring.
  std::string gl_version_string = gpu_info->gl_version_string;
  size_t pos = gl_version_string.find_last_of('-');
  if (pos == std::string::npos)
    return false;
  gpu_info->driver_version = gl_version_string.substr(pos + 1);
  return true;
}

}  // namespace gpu_info_collector
