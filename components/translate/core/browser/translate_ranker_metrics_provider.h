// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_BROWSER_TRANSLATE_RANKER_METRICS_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_BROWSER_TRANSLATE_RANKER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace translate {

// Provides metrics related to the translate ranker.
class TranslateRankerMetricsProvider : public metrics::MetricsProvider {
 public:
  TranslateRankerMetricsProvider();
  ~TranslateRankerMetricsProvider() override;

 private:
  // From metrics::MetricsProvider...
  void ProvideGeneralMetrics(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  DISALLOW_COPY_AND_ASSIGN(TranslateRankerMetricsProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_BROWSER_TRANSLATE_RANKER_METRICS_PROVIDER_H_
