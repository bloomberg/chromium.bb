// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_

namespace gfx {
class Image;
}

namespace favicon {

// An observer implemented by classes which are interested in event from
// FaviconDriver.
class FaviconDriverObserver {
 public:
  // Called when favicon |image| is retrieved from either web site or cached
  // storage.
  virtual void OnFaviconAvailable(const gfx::Image& image) = 0;

 protected:
  virtual ~FaviconDriverObserver() {}
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_OBSERVER_H_
