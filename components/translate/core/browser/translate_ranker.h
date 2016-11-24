// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"

namespace chrome_intelligence {
class TranslateRankerModel;
}

namespace metrics {
class TranslateEventProto;
}

namespace translate {

class TranslatePrefs;
class TranslateURLFetcher;

// Features used to enable ranker query, enforcement and logging. Note that
// enabling enforcement implies (forces) enabling queries.
extern const base::Feature kTranslateRankerQuery;
extern const base::Feature kTranslateRankerEnforcement;
extern const base::Feature kTranslateRankerLogging;

// If enabled, downloads a translate ranker model and uses it to determine
// whether the user should be given a translation prompt or not.
class TranslateRanker {
 public:
  ~TranslateRanker();

  // Returns true if query or enforcement is enabled.
  static bool IsEnabled();

  // Returns true if translate events logging is enabled.
  static bool IsLoggingEnabled();

  // Returns true if querying is enabled.
  static bool IsQueryEnabled();

  // Returns true if enforcement is enabled.
  static bool IsEnforcementEnabled();

  // Returns the singleton TranslateRanker instance.
  static TranslateRanker* GetInstance();

  // For testing only. Returns a TranslateRanker instance preloaded with a
  // TranslateRankerModel as defined by |model_data|.
  static std::unique_ptr<TranslateRanker> CreateForTesting(
      const std::string& model_data);

  // Initiates downloading of the assist model data. This is a NOP if the model
  // data has already been downloaded.
  void FetchModelData();

  // Returns true if executing the ranker model in the translation prompt
  // context described by |translate_prefs|, |src_lang|, |dst_lang| and possibly
  // other global browser context attributes suggests that the user should be
  // prompted as to whether translation should be performed.
  bool ShouldOfferTranslation(const TranslatePrefs& translate_prefs,
                              const std::string& src_lang,
                              const std::string& dst_lang);

  // Returns the model version (a date stamp) or 0 if there is no valid model.
  int GetModelVersion() const;

  // Caches the translate event.
  void RecordTranslateEvent(
      const metrics::TranslateEventProto& translate_event);

  // Transfers cached translate events to the given vector pointer and clears
  // the cache.
  void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* translate_events);

 private:
  // The ID which is assigned to the underlying URLFetcher.
  static constexpr int kFetcherId = 2;

  TranslateRanker();

  // Exposed for testing via FRIEND_TEST.
  double CalculateScore(double accept_ratio,
                        double decline_ratio,
                        double ignore_ratio,
                        const std::string& src_lang,
                        const std::string& dst_lang,
                        const std::string& app_locale,
                        const std::string& country);

  // Called when the model download has completed.
  void ParseModel(int id, bool success, const std::string& model_data);

  // The translation ranker model.
  std::unique_ptr<chrome_intelligence::TranslateRankerModel> model_;

  // A URL fetcher to download translation ranker model data.
  std::unique_ptr<TranslateURLFetcher> model_fetcher_;

  // The next time before which no new attempts to download the model should be
  // attempted.
  base::Time next_earliest_download_time_;

  // Tracks the last time the translate ranker attempted to download its model.
  // Used for UMA reporting of timing.
  base::Time download_start_time_;

  // Saved cache of translate event protos.
  std::vector<metrics::TranslateEventProto> translate_events_cache_;

  FRIEND_TEST_ALL_PREFIXES(TranslateRankerTest, CalculateScore);

  friend struct base::DefaultSingletonTraits<TranslateRanker>;

  DISALLOW_COPY_AND_ASSIGN(TranslateRanker);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
