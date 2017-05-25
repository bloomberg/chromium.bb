// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_LOAD_TERMINATION_LISTENER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_LOAD_TERMINATION_LISTENER_H_

#include "components/offline_pages/core/background/offliner.h"

namespace offline_pages {

// The OS-specific instance of this class is created and passed to
// BackgroundLoaderOffliner which takes lifetime ownership of it.
// When listener receives signals requiring immediate termination of loading,
// it should call Offliner::TerminateLoadIfInProgress().
class LoadTerminationListener {
 public:
  LoadTerminationListener() = default;
  virtual ~LoadTerminationListener() = default;

  // Called by Offliner when it takes ownership of this listener. Used to
  // cache pointer back to offliner to terminate the load.
  void set_offliner(Offliner* offliner) { offliner_ = offliner; }

 protected:
  // Raw pointer because this class is owned by Offliner.
  Offliner* offliner_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(LoadTerminationListener);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_LOAD_TERMINATION_LISTENER_H_