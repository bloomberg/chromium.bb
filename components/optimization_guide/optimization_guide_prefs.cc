// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {
namespace prefs {

// A dictionary pref that stores counts for the number of times a hint was
// attempted to be loaded and counts for the number of times a hint was actually
// loaded, broken down by hint source.
const char kHintLoadedCounts[] = "optimization_guide.hint_loaded_counts";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kHintLoadedCounts, PrefRegistry::LOSSY_PREF);
}

}  // namespace prefs
}  // namespace optimization_guide
