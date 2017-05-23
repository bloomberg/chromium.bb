// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_

namespace subresource_filter {

// NOTE: ActivationDecision backs a UMA histogram, so it is append-only.
enum class ActivationDecision : int {
  // The activation decision is unknown, or not known yet.
  UNKNOWN,

  // Subresource filtering was activated.
  ACTIVATED,

  // Did not activate because subresource filtering was disabled by the
  // highest priority configuration whose activation conditions were met.
  ACTIVATION_DISABLED,

  // Did not activate because the main frame document URL had an unsupported
  // scheme.
  UNSUPPORTED_SCHEME,

  // Did not activate because although there was a configuration whose
  // activation conditions were met, the main frame URL was whitelisted.
  URL_WHITELISTED,

  // Did not activate because the main frame document URL did not match the
  // activation conditions of any of enabled configurations.
  ACTIVATION_CONDITIONS_NOT_MET,

  // Max value for enum.
  ACTIVATION_DECISION_MAX
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_
