// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_ranker.h"

#include "components/metrics/proto/translate_event.pb.h"
#include "url/gurl.h"

namespace translate {
namespace testing {

MockTranslateRanker::MockTranslateRanker() {}

MockTranslateRanker::~MockTranslateRanker() {}

bool MockTranslateRanker::IsLoggingEnabled() {
  return is_logging_enabled_;
}

bool MockTranslateRanker::IsQueryEnabled() {
  return is_query_enabled_;
}

bool MockTranslateRanker::IsEnforcementEnabled() {
  return is_enforcement_enabled_;
}

int MockTranslateRanker::GetModelVersion() const {
  return model_version_;
}

bool MockTranslateRanker::ShouldOfferTranslation(
    const TranslatePrefs& /* translate_prefs */,
    const std::string& /* src_lang */,
    const std::string& /* dst_lang */) {
  return should_offer_translation_;
}

void MockTranslateRanker::AddTranslateEvent(
    const metrics::TranslateEventProto& translate_event,
    const GURL& /* url */) {
  event_cache_.push_back(translate_event);
}

void MockTranslateRanker::FlushTranslateEvents(
    std::vector<metrics::TranslateEventProto>* events) {
  events->swap(event_cache_);
  event_cache_.clear();
}

}  // namespace testing
}  // namespace translate
