// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/common/gpu_info.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

using base::debug::SetCrashKeyValueFuncT;
using base::debug::ClearCrashKeyValueFuncT;
using base::debug::SetCrashKeyValue;
using base::debug::ClearCrashKey;

const size_t kMaxNumCrashURLChunks = 8;
const size_t kMaxNumURLChunkValueLength = 255;
const char* kUrlChunkFormatStr = "url-chunk-%d";
const char* kGuidParamName = "guid";
const char* kGPUVendorIdParamName = "gpu-venid";
const char* kGPUDeviceIdParamName = "gpu-devid";
const char* kGPUDriverVersionParamName = "gpu-driver";
const char* kGPUPixelShaderVersionParamName = "gpu-psver";
const char* kGPUVertexShaderVersionParamName = "gpu-vsver";
const char* kGPUGLVersionParamName = "gpu-glver";
const char* kNumberOfViews = "num-views";
const char* kNumExtensionsName = "num-extensions";
const char* kExtensionNameFormat = "extension-%zu";
const char* kPrinterInfoNameFormat = "prn-info-%zu";

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

void SetActiveURLImpl(const GURL& url,
                      SetCrashKeyValueFuncT set_key_func,
                      ClearCrashKeyValueFuncT clear_key_func) {
  // First remove any old url chunks we might have lying around.
  for (size_t i = 0; i < kMaxNumCrashURLChunks; i++) {
    // On Windows the url-chunk items are 1-based, so match that.
    clear_key_func(base::StringPrintf(kUrlChunkFormatStr, i + 1));
  }

  const std::string& raw_url = url.possibly_invalid_spec();
  size_t raw_url_length = raw_url.length();

  // Bail on zero-length URLs.
  if (raw_url_length == 0) {
    return;
  }

  // Parcel the URL up into up to 8, 255 byte segments.
  size_t offset = 0;
  for (size_t i = 0;
       i < kMaxNumCrashURLChunks && offset < raw_url_length;
       ++i) {

    // On Windows the url-chunk items are 1-based, so match that.
    std::string key = base::StringPrintf(kUrlChunkFormatStr, i + 1);
    size_t length = std::min(kMaxNumURLChunkValueLength,
                             raw_url_length - offset);
    std::string value = raw_url.substr(offset, length);
    set_key_func(key, value);

    // Next chunk.
    offset += kMaxNumURLChunkValueLength;
  }
}

void SetClientIdImpl(const std::string& client_id,
                     SetCrashKeyValueFuncT set_key_func) {
  set_key_func(kGuidParamName, client_id);
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
  SetCrashKeyValue(kNumExtensionsName, base::IntToString(count));

  // Record up to |kMaxReportedActiveExtensions| extensions, clearing
  // keys if there aren't that many.
  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (size_t i = 0; i < kMaxReportedActiveExtensions; ++i) {
    std::string key = base::StringPrintf(kExtensionNameFormat, i);
    if (iter != extension_ids.end()) {
      SetCrashKeyValue(key, *iter);
      ++iter;
    } else {
      ClearCrashKey(key);
    }
  }
}

void SetGpuKeyValue(const char* param_name, const std::string& value_str,
                    SetCrashKeyValueFuncT set_key_func) {
  set_key_func(param_name, value_str);
}

void SetGpuInfoImpl(const content::GPUInfo& gpu_info,
                    SetCrashKeyValueFuncT set_key_func) {
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
  base::SplitString(printer_info, ';', &info);
  info.resize(kMaxReportedPrinterRecords);
  for (size_t i = 0; i < info.size(); ++i) {
    std::string key = base::StringPrintf(kPrinterInfoNameFormat, i);
    if (!info[i].empty()) {
      SetCrashKeyValue(key, info[i]);
    } else {
      ClearCrashKey(key);
    }
  }
}

void SetNumberOfViewsImpl(int number_of_views,
                          SetCrashKeyValueFuncT set_key_func) {
  std::string value = base::IntToString(number_of_views);
  set_key_func(kNumberOfViews, value);
}

void SetNumberOfViews(int number_of_views) {
  SetNumberOfViewsImpl(number_of_views, SetCrashKeyValue);
}

void SetCommandLine(const CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line)
    return;

  // These should match the corresponding strings in breakpad_win.cc.
  const char* kNumSwitchesKey = "num-switches";
  const char* kSwitchKeyFormat = "switch-%zu";  // 1-based.

  // Note the total number of switches, not including the exec path.
  const CommandLine::StringVector& argv = command_line->argv();
  SetCrashKeyValue(kNumSwitchesKey,
                   base::StringPrintf("%zu", argv.size() - 1));

  size_t key_i = 0;
  for (size_t i = 1; i < argv.size() && key_i < kMaxSwitches; ++i, ++key_i) {
    // TODO(shess): Skip boring switches.
    std::string key = base::StringPrintf(kSwitchKeyFormat, key_i + 1);
    SetCrashKeyValue(key, argv[i]);
  }

  // Clear out any stale keys.
  for (; key_i < kMaxSwitches; ++key_i) {
    std::string key = base::StringPrintf(kSwitchKeyFormat, key_i + 1);
    ClearCrashKey(key);
  }
}

void SetExperimentList(const std::vector<string16>& experiments) {
  // These should match the corresponding strings in breakpad_win.cc.
  const char* kNumExperimentsKey = "num-experiments";
  const char* kExperimentChunkFormat = "experiment-chunk-%zu";  // 1-based

  std::vector<string16> chunks;
  chrome_variations::GenerateVariationChunks(experiments, &chunks);

  // Store up to |kMaxReportedVariationChunks| chunks.
  for (size_t i = 0; i < kMaxReportedVariationChunks; ++i) {
    std::string key = base::StringPrintf(kExperimentChunkFormat, i + 1);
    if (i < chunks.size()) {
      std::string value = UTF16ToUTF8(chunks[i]);
      SetCrashKeyValue(key, value);
    } else {
      ClearCrashKey(key);
    }
  }

  // Make note of the total number of experiments, which may be greater than
  // what was able to fit in |kMaxReportedVariationChunks|. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  SetCrashKeyValue(kNumExperimentsKey,
                   base::StringPrintf("%zu", experiments.size()));
}

void SetChannel(const std::string& channel) {
  // This should match the corresponding string in breakpad_win.cc.
  const std::string kChannelKey = "channel";
  SetCrashKeyValue(kChannelKey, channel);
}

}  // namespace child_process_logging
