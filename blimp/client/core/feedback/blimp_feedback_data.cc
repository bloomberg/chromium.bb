// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/feedback/blimp_feedback_data.h"

namespace blimp {
namespace client {
const char kFeedbackSupportedKey[] = "Blimp Supported";

std::unordered_map<std::string, std::string> CreateBlimpFeedbackData() {
  std::unordered_map<std::string, std::string> data;
  data.insert(std::make_pair(kFeedbackSupportedKey, "true"));
  return data;
}

}  // namespace client
}  // namespace blimp
