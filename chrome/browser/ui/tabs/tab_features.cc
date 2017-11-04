// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_features.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model_impl.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"
#endif

const base::Feature kExperimentalTabControllerFeature{
    "ExperimentalTabController", base::FEATURE_DISABLED_BY_DEFAULT,
};

bool IsExperimentalTabStripEnabled() {
#if defined(OS_WIN)
  return base::FeatureList::IsEnabled(kExperimentalTabControllerFeature);
#else
  return false;
#endif
}

std::unique_ptr<TabStripModel> CreateTabStripModel(
    TabStripModelDelegate* delegate,
    Profile* profile) {
  // The experimental controller currently just crashes so don't inflict it on
  // people yet who may have accidentally triggered this feature.
#if defined(OS_WIN)
  if (IsExperimentalTabStripEnabled())
    return base::MakeUnique<TabStripModelExperimental>(delegate, profile);
#endif
  return base::MakeUnique<TabStripModelImpl>(delegate, profile);
}
