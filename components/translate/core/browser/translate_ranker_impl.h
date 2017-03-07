// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/ranker_model_loader.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "url/gurl.h"

namespace chrome_intelligence {
class RankerModel;
}  // namespace chrome_intelligence

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {

class TranslatePrefs;

// Features used to enable ranker query, enforcement and logging. Note that
// enabling enforcement implies (forces) enabling queries.
extern const base::Feature kTranslateRankerQuery;
extern const base::Feature kTranslateRankerEnforcement;
extern const base::Feature kTranslateRankerLogging;

struct TranslateRankerFeatures {
  TranslateRankerFeatures();

  TranslateRankerFeatures(int accepted,
                          int denied,
                          int ignored,
                          const std::string& src,
                          const std::string& dst,
                          const std::string& cntry,
                          const std::string& locale);

  TranslateRankerFeatures(const TranslatePrefs& prefs,
                          const std::string& src,
                          const std::string& dst,
                          const std::string& locale);

  ~TranslateRankerFeatures();

  void WriteTo(std::ostream& stream) const;

  // Input value used to generate the features.
  int accepted_count;
  int denied_count;
  int ignored_count;
  int total_count;

  // Used for inference.
  std::string src_lang;
  std::string dst_lang;
  std::string country;
  std::string app_locale;
  double accepted_ratio;
  double denied_ratio;
  double ignored_ratio;
};

// If enabled, downloads a translate ranker model and uses it to determine
// whether the user should be given a translation prompt or not.
class TranslateRankerImpl : public TranslateRanker {
 public:
  TranslateRankerImpl(const base::FilePath& model_path, const GURL& model_url);
  ~TranslateRankerImpl() override;

  // Get the file path of the translate ranker model, by default with a fixed
  // name within |data_dir|.
  static base::FilePath GetModelPath(const base::FilePath& data_dir);

  // Get the URL from which the download the translate ranker model, by default
  // from Finch.
  static GURL GetModelURL();

  // TranslateRanker...
  bool IsLoggingEnabled() override;
  bool IsQueryEnabled() override;
  bool IsEnforcementEnabled() override;
  int GetModelVersion() const override;
  bool ShouldOfferTranslation(const TranslatePrefs& translate_prefs,
                              const std::string& src_lang,
                              const std::string& dst_lang) override;
  void AddTranslateEvent(
      const metrics::TranslateEventProto& translate_event) override;
  void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) override;

  void OnModelAvailable(
      std::unique_ptr<chrome_intelligence::RankerModel> model);

  // Calculate the score given to |features| by the |model|.
  double CalculateScore(const TranslateRankerFeatures& features);

  // Check if the ModelLoader has been initialized. Used to test ModelLoader
  // logic.
  bool CheckModelLoaderForTesting();

 private:
  // Used to sanity check the threading of this ranker.
  base::SequenceChecker sequence_checker_;

  // A helper to load the translate ranker model from disk cache or a URL.
  std::unique_ptr<RankerModelLoader> model_loader_;

  // The translation ranker model.
  std::unique_ptr<chrome_intelligence::RankerModel> model_;

  // Saved cache of translate event protos.
  std::vector<metrics::TranslateEventProto> event_cache_;

  base::WeakPtrFactory<TranslateRankerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateRankerImpl);
};

}  // namespace translate

std::ostream& operator<<(std::ostream& stream,
                         const translate::TranslateRankerFeatures& features);

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_
