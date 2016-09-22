// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker_metrics_provider.h"

#include "base/metrics/sparse_histogram.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/translate_ranker.h"

namespace translate {

TranslateRankerMetricsProvider::TranslateRankerMetricsProvider() {}
TranslateRankerMetricsProvider::~TranslateRankerMetricsProvider() {}

void TranslateRankerMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* /* uma_proto */) {
  // Nothing to report if the translate ranker is disabled.
  if (!TranslateRanker::IsEnabled())
    return;

  const TranslateRanker* translate_ranker = TranslateRanker::GetInstance();
  if (translate_ranker != nullptr) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Translate.Ranker.Model.Version",
                                translate_ranker->GetModelVersion());
  }
}

}  // namespace translate
