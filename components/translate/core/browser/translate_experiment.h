// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EXPERIMENT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EXPERIMENT_H_

#include <string>

#include "base/gtest_prod_util.h"

namespace translate {

class TranslateExperiment {
 public:
  // Replaces the UI language for experiment purposes, if the experiment is
  // enabled. I.e. if the user participates in the experiment, and their UI
  // language differs from the predominantly spoken language in the area, this
  // changes |ui_lang| to the predominantly spoken language.
  static void OverrideUiLanguage(const std::string& country,
                                 std::string* ui_lang);

  // Check if a user-defined block needs to be overridden for |language|, i.e.
  // if the user is in the experiment and |language| matches |ui_language|.
  // This is necessary because the current Translate systems treats the UI
  // language as auto-blocked.
  static bool ShouldOverrideBlocking(const std::string& ui_language,
                                     const std::string& language);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateExperimentTest, TestInExperiment);

  static bool InExperiment();
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_EXPERIMENT_H_
