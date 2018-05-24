// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_reporter.h"

#include "base/debug/stack_trace.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_composite_reporter.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_metrics_reporter.h"

namespace contextual_suggestions {

ContextualSuggestionsReporterProvider::ContextualSuggestionsReporterProvider() =
    default;

ContextualSuggestionsReporterProvider::
    ~ContextualSuggestionsReporterProvider() = default;

std::unique_ptr<ContextualSuggestionsReporter>
ContextualSuggestionsReporterProvider::CreateReporter() {
  std::unique_ptr<ContextualSuggestionsCompositeReporter> reporter =
      std::make_unique<ContextualSuggestionsCompositeReporter>();
  reporter->AddOwnedReporter(
      std::make_unique<ContextualSuggestionsMetricsReporter>());
  return reporter;
}

ContextualSuggestionsReporter::ContextualSuggestionsReporter() = default;
ContextualSuggestionsReporter::~ContextualSuggestionsReporter() = default;

}  // namespace contextual_suggestions