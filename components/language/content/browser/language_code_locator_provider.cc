// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/language_code_locator_provider.h"

#include "base/feature_list.h"
#include "components/language/content/browser/language_code_locator.h"
#include "components/language/content/browser/regional_language_code_locator/regional_language_code_locator.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/common/language_experiments.h"

namespace language {
namespace {
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator_helper.h"
}  // namespace

std::unique_ptr<LanguageCodeLocator> GetLanguageCodeLocator() {
  if (base::FeatureList::IsEnabled(kImprovedGeoLanguageData)) {
    return std::make_unique<UlpLanguageCodeLocator>(
        std::make_unique<S2LangQuadTreeNode>(S2LangQuadTreeNode::Deserialize(
            GetLanguages(), GetTreeSerialized())));
  } else {
    return std::make_unique<RegionalLanguageCodeLocator>();
  }
}

}  // namespace language
