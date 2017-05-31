// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_

#include <string>

namespace gcm {
class GCMAppHandler;
}  // namespace gcm

namespace offline_pages {

class PrefetchGCMHandler;

// Main class and entry point for the Offline Pages Prefetching feature, that
// controls the lifetime of all major subcomponents of the prefetching system.
class PrefetchGCMHandler {
 public:
  virtual ~PrefetchGCMHandler() = default;

  // Returns the GCMAppHandler for this object.  Can return |nullptr| in unit
  // tests.
  virtual gcm::GCMAppHandler* AsGCMAppHandler() = 0;

  // The app ID to register with at the GCM layer.
  virtual std::string GetAppId() const = 0;

  // TODO(dewittj): Add methods for acquiring an Instance ID token to this
  // interface.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_
