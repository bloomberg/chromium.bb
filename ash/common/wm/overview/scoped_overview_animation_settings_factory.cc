// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"

#include "base/logging.h"

namespace ash {

// static
ScopedOverviewAnimationSettingsFactory*
    ScopedOverviewAnimationSettingsFactory::instance_ = nullptr;

// static
ScopedOverviewAnimationSettingsFactory*
ScopedOverviewAnimationSettingsFactory::Get() {
  return instance_;
}

ScopedOverviewAnimationSettingsFactory::
    ScopedOverviewAnimationSettingsFactory() {
  DCHECK(!instance_);
  instance_ = this;
}

ScopedOverviewAnimationSettingsFactory::
    ~ScopedOverviewAnimationSettingsFactory() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

}  // namespace ash
