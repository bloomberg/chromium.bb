// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_OBSERVER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_OBSERVER_H_

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

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_OBSERVER_H_
