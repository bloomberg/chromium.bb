// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/scheme_classifier_factory.h"

#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "net/url_request/url_request.h"

namespace athena {

namespace {

// The AutocompleteSchemeClassifier implementation for athena_main.
class AthenaShellSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  AthenaShellSchemeClassifier() {}
  virtual ~AthenaShellSchemeClassifier() {}

  // AutocompleteSchemeClassifier:
  virtual metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const OVERRIDE {
    if (net::URLRequest::IsHandledProtocol(scheme))
      return metrics::OmniboxInputType::URL;
    return metrics::OmniboxInputType::INVALID;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaShellSchemeClassifier);
};

}  // namespace

scoped_ptr<AutocompleteSchemeClassifier> CreateSchemeClassifier(
    content::BrowserContext* context) {
  return scoped_ptr<AutocompleteSchemeClassifier>(
      new AthenaShellSchemeClassifier());
}

}  // namespace athena
