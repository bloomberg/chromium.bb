// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/string_util.h"
#include "chrome/common/gpu_info.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

const int kMaxNumCrashURLChunks = 8;
const int kMaxNumURLChunkValueLength = 255;
const char *kUrlChunkFormatStr = "url-chunk-%d";
const char *kGuidParamName = "guid";
const char *kGPUVendorIdParamName = "vendid";
const char *kGPUDeviceIdParamName = "devid";
const char *kGPUDriverVersionParamName = "driver";
const char *kGPUPixelShaderVersionParamName = "psver";
const char *kGPUVertexShaderVersionParamName = "vsver";

static SetCrashKeyValueFuncPtr g_set_key_func;
static ClearCrashKeyValueFuncPtr g_clear_key_func;

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

  if (g_set_key_func)
    SetClientIdImpl(str, g_set_key_func);

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  // TODO(port)
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
                 UintToString(gpu_info.vendor_id()),
                 set_key_func);
  SetGpuKeyValue(kGPUDeviceIdParamName,
                 UintToString(gpu_info.device_id()),
                 set_key_func);
  SetGpuKeyValue(kGPUDriverVersionParamName,
                 WideToASCII(gpu_info.driver_version()),
                 set_key_func);
  SetGpuKeyValue(kGPUPixelShaderVersionParamName,
                 UintToString(gpu_info.pixel_shader_version()),
                 set_key_func);
  SetGpuKeyValue(kGPUVertexShaderVersionParamName,
                 UintToString(gpu_info.vertex_shader_version()),
                 set_key_func);
}

void SetGpuInfo(const GPUInfo& gpu_info) {
  if (g_set_key_func)
    SetGpuInfoImpl(gpu_info, g_set_key_func);
}

}  // namespace child_process_logging
