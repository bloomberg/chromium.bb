// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/translate_ranker.h"

namespace translate {

TranslateRankerMetricsProvider::TranslateRankerMetricsProvider() {}
TranslateRankerMetricsProvider::~TranslateRankerMetricsProvider() {}

void TranslateRankerMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  TranslateRanker* translate_ranker = TranslateRanker::GetInstance();
  if (translate_ranker != nullptr) {
    std::vector<metrics::TranslateEventProto> translate_events;
    translate_ranker->FlushTranslateEvents(&translate_events);
    for (metrics::TranslateEventProto& event : translate_events) {
      uma_proto->add_translate_event()->Swap(&event);
    }

    if (TranslateRanker::IsEnabled()) {
      // TODO(hamelphi): Remove this logging once we start using
      // TranslateEventProtos.
      UMA_HISTOGRAM_SPARSE_SLOWLY("Translate.Ranker.Model.Version",
                                  translate_ranker->GetModelVersion());
    }
  }
}

}  // namespace translate
