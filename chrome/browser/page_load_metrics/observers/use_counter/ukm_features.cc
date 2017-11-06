// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter/ukm_features.h"
#include "base/containers/flat_set.h"

// UKM-based UseCounter features (WebFeature) should be defined in
// opt_in_features list. Currently there have been no features opt-in for
// UKM-based UseCounter, which is why the list is empty.
bool IsAllowedUkmFeature(blink::mojom::WebFeature feature) {
  CR_DEFINE_STATIC_LOCAL(const base::flat_set<blink::mojom::WebFeature>,
                         opt_in_features, {});
  return opt_in_features.count(feature);
}
