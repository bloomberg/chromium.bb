// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_SCHEME_CLASSIFIER_H_
#define ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_SCHEME_CLASSIFIER_H_

#include "base/macros.h"
#include "components/omnibox/autocomplete_scheme_classifier.h"

namespace athena {

// The AutocompleteSchemeClassifier implementation for athena_main.
class AthenaShellSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  AthenaShellSchemeClassifier();
  ~AthenaShellSchemeClassifier() override;

  // AutocompleteSchemeClassifier:
  metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaShellSchemeClassifier);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_SCHEME_CLASSIFIER_H_
