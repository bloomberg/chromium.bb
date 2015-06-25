// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_UI_H_
#define MANDOLINE_UI_BROWSER_BROWSER_UI_H_

namespace mojo {
class ApplicationImpl;
class View;
}

namespace mandoline {

class Browser;

class BrowserUI {
 public:
  virtual ~BrowserUI() {}
  static BrowserUI* Create(Browser* browser,
                           mojo::ApplicationImpl* application_impl);

  // Called when the Browser UI is embedded within the specified view.
  // BrowserUI is destroyed prior to |root| being destroyed. That is, the
  // BrowserUI implementations can assume |root| is never deleted out from under
  // them.
  virtual void Init(mojo::View* root) = 0;

  virtual void OnURLChanged() = 0;
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_UI_H_
