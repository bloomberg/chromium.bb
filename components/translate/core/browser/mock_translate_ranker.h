// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/translate/core/browser/translate_ranker.h"

class GURL;

namespace metrics {
class TranslateEventProto;
}

namespace translate {

class TranslatePrefs;

namespace testing {

class MockTranslateRanker : public TranslateRanker {
 public:
  MockTranslateRanker();
  ~MockTranslateRanker() override;

  void set_is_logging_enabled(bool val) { is_logging_enabled_ = val; }
  void set_is_query_enabled(bool val) { is_query_enabled_ = val; }
  void set_is_enforcement_enabled(bool val) { is_enforcement_enabled_ = val; }
  void set_model_version(int val) { model_version_ = val; }
  void set_should_offer_translation(bool val) {
    should_offer_translation_ = val;
  }

  // TranslateRanker Implementation:
  bool IsLoggingEnabled() override;
  bool IsQueryEnabled() override;
  bool IsEnforcementEnabled() override;
  int GetModelVersion() const override;
  bool ShouldOfferTranslation(const TranslatePrefs& translate_prefs,
                              const std::string& src_lang,
                              const std::string& dst_lang) override;
  void AddTranslateEvent(const metrics::TranslateEventProto& translate_event,
                         const GURL& url) override;
  void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) override;

 private:
  std::vector<metrics::TranslateEventProto> event_cache_;

  bool is_logging_enabled_ = false;
  bool is_query_enabled_ = false;
  bool is_enforcement_enabled_ = false;
  bool model_version_ = 0;
  bool should_offer_translation_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockTranslateRanker);
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_
