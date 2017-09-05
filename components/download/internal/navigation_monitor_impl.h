// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_NAVIGATION_MONITOR_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_NAVIGATION_MONITOR_IMPL_H_

#include "base/macros.h"
#include "components/download/public/navigation_monitor.h"

namespace download {

class NavigationMonitorImpl : public NavigationMonitor {
 public:
  NavigationMonitorImpl();
  ~NavigationMonitorImpl() override;

  // NavigationMonitor implementation.
  void SetObserver(NavigationMonitor::Observer* observer) override;
  void OnNavigationEvent(NavigationEvent event) override;

 private:
  NavigationMonitor::Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NavigationMonitorImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_NAVIGATION_MONITOR_IMPL_H_
