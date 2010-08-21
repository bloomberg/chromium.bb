// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"
#include "base/sys_string_conversions.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <OpenGL/GL.h>

namespace gpu_info_collector {

static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                       CFStringRef propertyName) {
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}

static void CFReleaseIf(CFTypeRef type_ref) {
  if (type_ref)
    CFRelease(type_ref);
}

static UInt32 IntValueOfCFData(CFDataRef data_ref) {
  UInt32 value = 0;

  if (data_ref) {
    const UInt32 *value_pointer =
        reinterpret_cast<const UInt32*>(CFDataGetBytePtr(data_ref));
    if (value_pointer != NULL)
      value = *value_pointer;
  }

  return value;
}

static void CollectVideoCardInfo(CGDirectDisplayID displayID,
                                 int *vendorID,
                                 int *deviceID) {
  io_registry_entry_t dspPort = CGDisplayIOServicePort(displayID);

  CFTypeRef vendorIDRef = SearchPortForProperty(dspPort, CFSTR("vendor-id"));
  if (vendorID) *vendorID = IntValueOfCFData((CFDataRef)vendorIDRef);

  CFTypeRef deviceIDRef = SearchPortForProperty(dspPort, CFSTR("device-id"));
  if (deviceID) *deviceID = IntValueOfCFData((CFDataRef)deviceIDRef);

  CFReleaseIf(vendorIDRef);
  CFReleaseIf(deviceIDRef);
}

// Return a pointer to the last character with value c in string s.
// Returns NULL if c is not found.
static char* FindLastChar(char *s, char c) {
  char *s_found = NULL;

  while (*s != '\0') {
    if (*s == c)
      s_found = s;
    s++;
  }
  return s_found;
}


bool CollectGraphicsInfo(GPUInfo& gpu_info) {
  int vendor_id = 0;
  int device_id = 0;
  std::wstring driver_version = L"";
  uint32 pixel_shader_version = 0u;
  uint32 vertex_shader_version = 0u;
  uint32 gl_version;

  CollectVideoCardInfo(kCGDirectMainDisplay, &vendor_id, &device_id);

  char *gl_version_string = (char*)glGetString(GL_VERSION);
  char *gl_extensions_string = (char*)glGetString(GL_EXTENSIONS);

  if ((gl_version_string == NULL) || (gl_extensions_string == NULL))
    return false;

  // Get the OpenGL version from the start of the string.
  int gl_major = 0, gl_minor = 0;
  sscanf(gl_version_string, "%u.%u",  &gl_major, &gl_minor);

  // Get the OpenGL driver version.
  // Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  char *s = FindLastChar(gl_version_string, '-');
  if (s) {
    NSString *driver_version_ns = [[NSString alloc ] initWithUTF8String:s + 1];
    driver_version = base::SysNSStringToWide(driver_version_ns);
    [driver_version_ns release];
  }

  // Get the HLSL version.
  // On OpenGL 1.x it's 1.0 if the GL_ARB_shading_language_100 extension is
  // present.
  // On OpenGL 2.x  it's a matter of getting the GL_SHADING_LANGUAGE_VERSION
  // string.
  int gl_hlsl_major = 0, gl_hlsl_minor = 0;
  if ((gl_major == 1) &&
      strstr(gl_extensions_string, "GL_ARB_shading_language_100")) {
    gl_hlsl_major = 1;
    gl_hlsl_minor = 0;
  } else if (gl_major >= 2) {
    char* glsl_version_string = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (glsl_version_string) {
      sscanf(glsl_version_string, "%u.%u", &gl_hlsl_major, &gl_hlsl_minor);
    }
  }
  pixel_shader_version = ((gl_hlsl_major << 16) & 0xffff0000)
                         + (gl_hlsl_minor & 0x0000ffff);

  // OpenGL doesn't have separate versions for pixel and vertex shader
  // languages, so set them both to the GL_SHADING_LANGUAGE_VERSION value.
  vertex_shader_version = pixel_shader_version;

  gl_version = ((gl_major << 16) & 0xffff0000)
               + (gl_minor & 0x0000ffff);

  gpu_info.SetGraphicsInfo(vendor_id, device_id, driver_version,
                           pixel_shader_version, vertex_shader_version,
                           gl_version);
  return true;
}

}  // namespace gpu_info_collector
