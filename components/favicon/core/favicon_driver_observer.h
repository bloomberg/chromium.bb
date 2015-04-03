// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_

#include "base/macros.h"

namespace gfx {
class Image;
}

namespace favicon {

// An observer implemented by classes which are interested in event from
// FaviconDriver.
class FaviconDriverObserver {
 public:
  FaviconDriverObserver() {}
  virtual ~FaviconDriverObserver() {}

  // Called when favicon |image| is retrieved from either web site or cached
  // storage.
  virtual void OnFaviconAvailable(const gfx::Image& image) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconDriverObserver);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_
