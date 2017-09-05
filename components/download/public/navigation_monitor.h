// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_NAVIGATION_MONITOR_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_NAVIGATION_MONITOR_H_

#include "components/keyed_service/core/keyed_service.h"

namespace download {

// Enum used to describe navigation event from WebContentsObserver.
enum class NavigationEvent {
  START_NAVIGATION = 0,
  NAVIGATION_COMPLETE = 1,
};

// NavigationMonitor receives and forwards navigation events from any web
// contents to download service.
//
// NavigationMonitor outlives any WebContentsObserver that send navigation
// events to it.
//
// NavigationMonitor does NOT has ownership of WebContentsObserver, and is
// essentially a decoupled singleton that glues download service with
// WebContents and WebContentsObserver.
class NavigationMonitor : public KeyedService {
 public:
  // Used to propagates the navigation events.
  class Observer {
   public:
    virtual void OnNavigationEvent() = 0;
    virtual ~Observer() = default;
  };

  // Start to listen to navigation event.
  virtual void SetObserver(NavigationMonitor::Observer* observer) = 0;

  // Called when navigation events happen.
  virtual void OnNavigationEvent(NavigationEvent event) = 0;

 protected:
  ~NavigationMonitor() override{};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_NAVIGATION_MONITOR_H_
