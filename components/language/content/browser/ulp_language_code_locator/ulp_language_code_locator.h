// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/language/content/browser/language_code_locator.h"

class S2LangQuadTreeNode;

namespace language {

class UlpLanguageCodeLocator : public LanguageCodeLocator {
 public:
  UlpLanguageCodeLocator(std::unique_ptr<S2LangQuadTreeNode> root);
  ~UlpLanguageCodeLocator() override;

  // LanguageCodeLocator implementation.
  std::vector<std::string> GetLanguageCode(double latitude,
                                           double longitude) const override;

 private:
  std::unique_ptr<S2LangQuadTreeNode> root_;
  DISALLOW_COPY_AND_ASSIGN(UlpLanguageCodeLocator);
};
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_
