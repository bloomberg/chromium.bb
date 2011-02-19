// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "app/gfx/gl/gl_interface.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>

namespace {

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

}  // namespace anonymous

namespace gpu_info_collector {

bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  gpu_info->SetCanLoseContext(
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2);
  gpu_info->SetLevel(GPUInfo::kComplete);
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectPreliminaryGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  gpu_info->SetLevel(GPUInfo::kPartial);

  bool rt = true;
  if (!CollectVideoCardInfo(gpu_info))
    rt = false;

  return rt;
}

bool CollectVideoCardInfo(GPUInfo* gpu_info) {
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

  gpu_info->SetVideoCardInfo(vendor_id, device_id);
  return true;
}

bool CollectDriverInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // Extract the OpenGL driver version string from the GL_VERSION string.
  // Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  // Use some jiggery-pokery to turn that utf8 string into a std::wstring.
  std::string gl_version_string = gpu_info->gl_version_string();
  size_t pos = gl_version_string.find_last_of('-');
  if (pos == std::string::npos)
    return false;
  gpu_info->SetDriverInfo("", gl_version_string.substr(pos + 1));
  return true;
}

}  // namespace gpu_info_collector
