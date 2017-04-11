// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_STORE_H_

#include "base/callback.h"
#include "base/macros.h"

namespace feature_engagement_tracker {

// Store represents the storage engine behind the FeatureEngagementTracker.
class Store {
 public:
  // TODO(nyquist): Add vector of all events to result callback.
  using OnLoadedCallback = base::Callback<void(bool success)>;

  virtual ~Store() = default;

  // Loads the database from storage and asynchronously posts the result back.
  virtual void Load(const OnLoadedCallback& callback) = 0;

  // Returns whether the database is ready, i.e. whether it has been fully
  // loaded.
  virtual bool IsReady() const = 0;

 protected:
  Store() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Store);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_STORE_H_
