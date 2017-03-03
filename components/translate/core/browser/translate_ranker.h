// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {

class TranslatePrefs;

// If enabled, downloads a translate ranker model and uses it to determine
// whether the user should be given a translation prompt or not.
class TranslateRanker : public KeyedService {
 public:
  TranslateRanker() = default;

  // Returns true if translate events logging is enabled.
  virtual bool IsLoggingEnabled() = 0;

  // Returns true if enforcement is enabled.
  virtual bool IsEnforcementEnabled() = 0;

  // Returns true if querying is enabled. Enabling enforcement will
  // automatically enable Query.
  virtual bool IsQueryEnabled() = 0;

  // Returns the version id for the ranker model.
  virtual int GetModelVersion() const = 0;

  // Returns true if executing the ranker model in the translation prompt
  // context described by |translate_prefs|, |src_lang|, |dst_lang| and possibly
  // other global browser context attributes suggests that the user should be
  // prompted as to whether translation should be performed.
  virtual bool ShouldOfferTranslation(const TranslatePrefs& translate_prefs,
                                      const std::string& src_lang,
                                      const std::string& dst_lang) = 0;

  // Caches the translate event.
  virtual void AddTranslateEvent(
      const metrics::TranslateEventProto& translate_event,
      const GURL& url) = 0;

  // Transfers cached translate events to the given vector pointer and clears
  // the cache.
  virtual void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateRanker);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
