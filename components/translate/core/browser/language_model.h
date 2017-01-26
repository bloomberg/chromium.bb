// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_MODEL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_MODEL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace translate {

// Collects data about languages in which the user reads the web and provides
// access to current estimated language preferences. The past behaviour is
// discounted so that this model reflects changes in browsing habits. This model
// does not have to contain all languages that ever appeared in user's browsing,
// languages with insignificant frequency are removed, eventually.
class LanguageModel : public KeyedService {
 public:
  struct LanguageInfo {
    LanguageInfo() = default;
    LanguageInfo(const std::string& language_code, float frequency)
        : language_code(language_code), frequency(frequency) {}

    // The ISO 639 language code.
    std::string language_code;

    // The current estimated frequency of the language share, a number between 0
    // and 1 (can be understood as the probability that the next page the user
    // opens is in this language). Frequencies over all LanguageInfos from
    // GetTopLanguages() sum to 1 (unless there are no top languages, yet).
    float frequency = 0.0f;
  };

  explicit LanguageModel(PrefService* pref_service);
  ~LanguageModel() override;

  // Registers profile prefs for the model.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns a list of the languages currently tracked by the model, sorted by
  // frequency in decreasing order. The list is empty, if the model has not
  // enough data points.
  std::vector<LanguageInfo> GetTopLanguages() const;

  // Returns the estimated frequency for the given language or 0 if the language
  // is not among the top languages kept in the model.
  float GetLanguageFrequency(const std::string& language_code) const;

  // Informs the model that a page with the given language has been visited.
  void OnPageVisited(const std::string& language_code);

  // Reflect in the model that history from |begin| to |end| gets cleared.
  void ClearHistory(base::Time begin, base::Time end);

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(LanguageModel);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_MODEL_H_
