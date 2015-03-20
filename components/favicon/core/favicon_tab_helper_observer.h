// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_TAB_HELPER_OBSERVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_TAB_HELPER_OBSERVER_H_

namespace gfx {
class Image;
}

// An observer implemented by classes which are interested in envent in
// FaviconTabHelper.
class FaviconTabHelperObserver {
 public:
  // Called when favicon |image| is retrieved from either web site or history
  // backend.
  virtual void OnFaviconAvailable(const gfx::Image& image) = 0;

 protected:
  virtual ~FaviconTabHelperObserver() {}
};

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_TAB_HELPER_OBSERVER_H_
