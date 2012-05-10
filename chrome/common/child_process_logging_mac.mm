// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/common/gpu_info.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

using base::mac::SetCrashKeyValueFuncPtr;
using base::mac::ClearCrashKeyValueFuncPtr;
using base::mac::SetCrashKeyValue;
using base::mac::ClearCrashKey;

const int kMaxNumCrashURLChunks = 8;
const int kMaxNumURLChunkValueLength = 255;
const char *kUrlChunkFormatStr = "url-chunk-%d";
const char *kGuidParamName = "guid";
const char *kGPUVendorIdParamName = "gpu-venid";
const char *kGPUDeviceIdParamName = "gpu-devid";
const char *kGPUDriverVersionParamName = "gpu-driver";
const char *kGPUPixelShaderVersionParamName = "gpu-psver";
const char *kGPUVertexShaderVersionParamName = "gpu-vsver";
const char *kGPUGLVersionParamName = "gpu-glver";
const char *kNumberOfViews = "num-views";
NSString* const kNumExtensionsName = @"num-extensions";
NSString* const kExtensionNameFormat = @"extension-%d";
NSString* const kPrinterInfoNameFormat = @"prn-info-%zu";

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

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
  SetActiveURLImpl(url, SetCrashKeyValue, ClearCrashKey);
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
    SetClientIdImpl(str, SetCrashKeyValue);

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  // Log the count separately to track heavy users.
  const int count = static_cast<int>(extension_ids.size());
  SetCrashKeyValue(kNumExtensionsName,
                   [NSString stringWithFormat:@"%i", count]);

  // Record up to |kMaxReportedActiveExtensions| extensions, clearing
  // keys if there aren't that many.
  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (int i = 0; i < kMaxReportedActiveExtensions; ++i) {
    NSString* key = [NSString stringWithFormat:kExtensionNameFormat, i];
    if (iter != extension_ids.end()) {
      SetCrashKeyValue(key, [NSString stringWithUTF8String:iter->c_str()]);
      ++iter;
    } else {
      ClearCrashKey(key);
    }
  }
}

void SetGpuKeyValue(const char* param_name, const std::string& value_str,
                    SetCrashKeyValueFuncPtr set_key_func) {
  NSString *key = [NSString stringWithUTF8String:param_name];
  NSString *value = [NSString stringWithUTF8String:value_str.c_str()];
  set_key_func(key, value);
}

void SetGpuInfoImpl(const content::GPUInfo& gpu_info,
                    SetCrashKeyValueFuncPtr set_key_func) {
  SetGpuKeyValue(kGPUVendorIdParamName,
                 base::StringPrintf("0x%04x", gpu_info.gpu.vendor_id),
                 set_key_func);
  SetGpuKeyValue(kGPUDeviceIdParamName,
                 base::StringPrintf("0x%04x", gpu_info.gpu.device_id),
                 set_key_func);
  SetGpuKeyValue(kGPUDriverVersionParamName,
                 gpu_info.driver_version,
                 set_key_func);
  SetGpuKeyValue(kGPUPixelShaderVersionParamName,
                 gpu_info.pixel_shader_version,
                 set_key_func);
  SetGpuKeyValue(kGPUVertexShaderVersionParamName,
                 gpu_info.vertex_shader_version,
                 set_key_func);
  SetGpuKeyValue(kGPUGLVersionParamName,
                 gpu_info.gl_version,
                 set_key_func);
}

void SetGpuInfo(const content::GPUInfo& gpu_info) {
    SetGpuInfoImpl(gpu_info, SetCrashKeyValue);
}

void SetPrinterInfo(const char* printer_info) {
  std::vector<std::string> info;
  base::SplitString(printer_info, L';', &info);
  info.resize(kMaxReportedPrinterRecords);
  for (size_t i = 0; i < info.size(); ++i) {
    NSString* key = [NSString stringWithFormat:kPrinterInfoNameFormat, i];
    ClearCrashKey(key);
    if (!info[i].empty()) {
      NSString *value = [NSString stringWithUTF8String:info[i].c_str()];
      SetCrashKeyValue(key, value);
    }
  }
}

void SetNumberOfViewsImpl(int number_of_views,
                          SetCrashKeyValueFuncPtr set_key_func) {
  NSString *key = [NSString stringWithUTF8String:kNumberOfViews];
  NSString *value = [NSString stringWithFormat:@"%d", number_of_views];
  set_key_func(key, value);
}

void SetNumberOfViews(int number_of_views) {
    SetNumberOfViewsImpl(number_of_views, SetCrashKeyValue);
}

void SetCommandLine(const CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line)
    return;

  // These should match the corresponding strings in breakpad_win.cc.
  NSString* const kNumSwitchesKey = @"num-switches";
  NSString* const kSwitchKeyFormat = @"switch-%zu";  // 1-based.

  // Note the total number of switches, not including the exec path.
  const CommandLine::StringVector& argv = command_line->argv();
  SetCrashKeyValue(kNumSwitchesKey,
                   [NSString stringWithFormat:@"%zu", argv.size() - 1]);

  size_t key_i = 0;
  for (size_t i = 1; i < argv.size() && key_i < kMaxSwitches; ++i) {
    // TODO(shess): Skip boring switches.
    NSString* key = [NSString stringWithFormat:kSwitchKeyFormat, (key_i + 1)];
    NSString* value = base::SysUTF8ToNSString(argv[i]);
    SetCrashKeyValue(key, value);
    key_i++;
  }

  // Clear out any stale keys.
  for (; key_i < kMaxSwitches; ++key_i) {
    NSString* key = [NSString stringWithFormat:kSwitchKeyFormat, (key_i + 1)];
    ClearCrashKey(key);
  }
}

void SetExperimentList(const std::vector<string16>& state) {
  // TODO(mad): Implement this.
}

void SetChannel(const std::string& channel) {
  // This should match the corresponding string in breakpad_win.cc.
  NSString* const kChannelKey = @"channel";

  SetCrashKeyValue(kChannelKey, base::SysUTF8ToNSString(channel));
}

}  // namespace child_process_logging
