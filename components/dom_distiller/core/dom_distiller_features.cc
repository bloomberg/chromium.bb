// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_features.h"

#include <string>

#include "base/command_line.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/variations/variations_associated_data.h"

namespace dom_distiller {
namespace {
const char kFieldTrialName[] = "EnhancedBookmarks";
}

bool IsEnableDomDistillerSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller)) {
    return true;
  }
  if (variations::GetVariationParamValue(kFieldTrialName,
                                         "enable-dom-distiller") == "1") {
    return true;
  }
  return false;
}

bool IsEnableSyncArticlesSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSyncArticles)) {
    return true;
  }
  if (variations::GetVariationParamValue(kFieldTrialName,
                                         "enable-sync-articles") == "1") {
    return true;
  }
  return false;
}

}  // namespace dom_distiller
