// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_ranker_metrics_provider.h"

#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/translate/translate_ranker_factory.h"
#include "ios/web/public/browser_state.h"

namespace translate {

void TranslateRankerMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (auto* state : browser_states) {
    TranslateRanker* ranker =
        TranslateRankerFactory::GetInstance()->GetForBrowserState(state);
    DCHECK(ranker);
    std::vector<metrics::TranslateEventProto> translate_events;
    ranker->FlushTranslateEvents(&translate_events);

    for (auto& event : translate_events) {
      uma_proto->add_translate_event()->Swap(&event);
    }
  }
}

}  // namespace translate
