// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_UI_H_
#define MANDOLINE_UI_BROWSER_BROWSER_UI_H_

class GURL;

namespace mojo {
class ApplicationConnection;
class ApplicationImpl;
class View;
}

namespace mandoline {

class BrowserManager;

class BrowserUI {
 public:
  virtual ~BrowserUI() {}
  static BrowserUI* Create(mojo::ApplicationImpl* application_impl,
                           BrowserManager* manager);

  // Loads the specified URL in the active tab.
  virtual void LoadURL(const GURL& url) = 0;
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_UI_H_
