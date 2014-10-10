// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/shell/athena_shell_scheme_classifier.h"

#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "net/url_request/url_request.h"

using metrics::OmniboxInputType::Type;

namespace athena {

AthenaShellSchemeClassifier::AthenaShellSchemeClassifier() {
}

AthenaShellSchemeClassifier::~AthenaShellSchemeClassifier() {
}

Type AthenaShellSchemeClassifier::GetInputTypeForScheme(
    const std::string& scheme) const {
  if (net::URLRequest::IsHandledProtocol(scheme))
    return metrics::OmniboxInputType::URL;
  return metrics::OmniboxInputType::INVALID;
}

}  // namespace athena
