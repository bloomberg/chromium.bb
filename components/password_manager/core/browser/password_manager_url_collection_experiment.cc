// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

namespace password_manager {
namespace urls_collection_experiment {

bool ShouldShowBubble(PrefService* prefs) {
  // TODO(melandory) Make descision based on Finch experiment.
  // "Do not show" is the default case.
  return false;
}

}  // namespace urls_collection_experiment
}  // namespace password_manager
