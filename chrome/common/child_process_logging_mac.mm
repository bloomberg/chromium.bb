// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/gpu_info.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

const int kMaxNumCrashURLChunks = 8;
const int kMaxNumURLChunkValueLength = 255;
const char *kUrlChunkFormatStr = "url-chunk-%d";
const char *kGuidParamName = "guid";
const char *kGPUVendorIdParamName = "gpu-vendid";
const char *kGPUDeviceIdParamName = "gpu-devid";
const char *kGPUDriverVersionParamName = "gpu-driver";
const char *kGPUPixelShaderVersionParamName = "gpu-psver";
const char *kGPUVertexShaderVersionParamName = "gpu-vsver";
const char *kGPUGLVersionParamName = "gpu-glver";
const char *kNumberOfViews = "num-views";
NSString* const kNumExtensionsName = @"num-extensions";
NSString* const kExtensionNameFormat = @"extension-%d";

static SetCrashKeyValueFuncPtr g_set_key_func;
static ClearCrashKeyValueFuncPtr g_clear_key_func;

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

void SetCrashKeyFunctions(SetCrashKeyValueFuncPtr set_key_func,
                          ClearCrashKeyValueFuncPtr clear_key_func) {
  g_set_key_func = set_key_func;
  g_clear_key_func = clear_key_func;
}

void SetActiveURLImpl(const GURL& url,
                      SetCrashKeyValueFuncPtr set_key_func,
                      ClearCrashKeyValueFuncPtr clear_key_func) {

  NSString *kUrlChunkFormatStr_utf8 = [NSString
      stringWithUTF8String:kUrlChunkFormatStr];

  // First remove any old url chunks we might have lying around.
  for (int i = 0; i < kMaxNumCrashURLChunks; i++) {
    // On Windows the url-chunk items are 1-based, so match that.
    NSString *key = [NSString stringWithFormat:kUrlChunkFormatStr_utf8, i+1];
    clear_key_func(key);
  }

  const std::string& raw_url_utf8 = url.possibly_invalid_spec();
  NSString *raw_url = [NSString stringWithUTF8String:raw_url_utf8.c_str()];
  size_t raw_url_length = [raw_url length];

  // Bail on zero-length URLs.
  if (raw_url_length == 0) {
    return;
  }

  // Parcel the URL up into up to 8, 255 byte segments.
  size_t start_ofs = 0;
  for (int i = 0;
       i < kMaxNumCrashURLChunks && start_ofs < raw_url_length;
       ++i) {

    // On Windows the url-chunk items are 1-based, so match that.
    NSString *key = [NSString stringWithFormat:kUrlChunkFormatStr_utf8, i+1];
    NSRange range;
    range.location = start_ofs;
    range.length = std::min((size_t)kMaxNumURLChunkValueLength,
                            raw_url_length - start_ofs);
    NSString *value = [raw_url substringWithRange:range];
    set_key_func(key, value);

    // Next chunk.
    start_ofs += kMaxNumURLChunkValueLength;
  }
}

void SetClientIdImpl(const std::string& client_id,
                     SetCrashKeyValueFuncPtr set_key_func) {
  NSString *key = [NSString stringWithUTF8String:kGuidParamName];
  NSString *value = [NSString stringWithUTF8String:client_id.c_str()];
  set_key_func(key, value);
}

void SetActiveURL(const GURL& url) {
  if (g_set_key_func && g_clear_key_func)
    SetActiveURLImpl(url, g_set_key_func, g_clear_key_func);
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
  if (g_set_key_func)
    SetClientIdImpl(str, g_set_key_func);

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  if (!g_set_key_func)
    return;

  // Log the count separately to track heavy users.
  const int count = static_cast<int>(extension_ids.size());
  g_set_key_func(kNumExtensionsName, [NSString stringWithFormat:@"%i", count]);

  // Record up to |kMaxReportedActiveExtensions| extensions, clearing
  // keys if there aren't that many.
  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (int i = 0; i < kMaxReportedActiveExtensions; ++i) {
    NSString* key = [NSString stringWithFormat:kExtensionNameFormat, i];
    if (iter != extension_ids.end()) {
      g_set_key_func(key, [NSString stringWithUTF8String:iter->c_str()]);
      ++iter;
    } else {
      g_clear_key_func(key);
    }
  }
}

void SetGpuKeyValue(const char* param_name, const std::string& value_str,
                    SetCrashKeyValueFuncPtr set_key_func) {
  NSString *key = [NSString stringWithUTF8String:param_name];
  NSString *value = [NSString stringWithUTF8String:value_str.c_str()];
  set_key_func(key, value);
}

void SetGpuInfoImpl(const GPUInfo& gpu_info,
                    SetCrashKeyValueFuncPtr set_key_func) {
  SetGpuKeyValue(kGPUVendorIdParamName,
                 base::StringPrintf("0x%04x", gpu_info.vendor_id),
                 set_key_func);
  SetGpuKeyValue(kGPUDeviceIdParamName,
                 base::StringPrintf("0x%04x", gpu_info.device_id),
                 set_key_func);
  SetGpuKeyValue(kGPUDriverVersionParamName,
                 gpu_info.driver_version,
                 set_key_func);
  SetGpuKeyValue(kGPUPixelShaderVersionParamName,
                 base::UintToString(gpu_info.pixel_shader_version),
                 set_key_func);
  SetGpuKeyValue(kGPUVertexShaderVersionParamName,
                 base::UintToString(gpu_info.vertex_shader_version),
                 set_key_func);
  SetGpuKeyValue(kGPUGLVersionParamName,
                 base::UintToString(gpu_info.gl_version),
                 set_key_func);
}

void SetGpuInfo(const GPUInfo& gpu_info) {
  if (g_set_key_func)
    SetGpuInfoImpl(gpu_info, g_set_key_func);
}


void SetNumberOfViewsImpl(int number_of_views,
                          SetCrashKeyValueFuncPtr set_key_func) {
  NSString *key = [NSString stringWithUTF8String:kNumberOfViews];
  NSString *value = [NSString stringWithFormat:@"%d", number_of_views];
  set_key_func(key, value);
}

void SetNumberOfViews(int number_of_views) {
  if (g_set_key_func)
    SetNumberOfViewsImpl(number_of_views, g_set_key_func);
}

}  // namespace child_process_logging
