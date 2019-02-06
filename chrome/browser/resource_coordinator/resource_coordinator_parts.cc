// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"

#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace resource_coordinator {

ResourceCoordinatorParts::ResourceCoordinatorParts()
    : page_signal_receiver_(
          performance_manager::PerformanceManager::GetInstance()
              ? std::make_unique<resource_coordinator::PageSignalReceiver>()
              : nullptr)
#if !defined(OS_ANDROID)
      ,
      tab_manager_(page_signal_receiver_.get(), &tab_load_tracker_),
      tab_lifecycle_unit_source_(tab_manager_.intervention_policy_database(),
                                 tab_manager_.usage_clock(),
                                 page_signal_receiver_.get())
#endif
{
#if !defined(OS_ANDROID)
  tab_lifecycle_unit_source_.AddObserver(&tab_manager_);
#endif
}

ResourceCoordinatorParts::~ResourceCoordinatorParts() = default;

}  // namespace resource_coordinator
