// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include "base/metrics/histogram_macros.h"

namespace app_list {

void LogConfigurationError(ConfigurationError error) {
  UMA_HISTOGRAM_ENUMERATION("RecurrenceRanker.ConfigurationError", error);
}

void LogSerializationError(SerializationError error) {
  UMA_HISTOGRAM_ENUMERATION("RecurrenceRanker.SerializationError", error);
}

void LogUsageError(UsageError error) {
  UMA_HISTOGRAM_ENUMERATION("RecurrenceRanker.UsageError", error);
}

}  // namespace app_list
