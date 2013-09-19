// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

using base::debug::SetCrashKeyValueFuncT;
using base::debug::ClearCrashKeyValueFuncT;
using base::debug::SetCrashKeyValue;
using base::debug::ClearCrashKey;

const char* kGuidParamName = "guid";

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

void SetClientIdImpl(const std::string& client_id,
                     SetCrashKeyValueFuncT set_key_func) {
  set_key_func(kGuidParamName, client_id);
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

}  // namespace child_process_logging
