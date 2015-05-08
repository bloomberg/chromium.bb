// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_UI_H_
#define MANDOLINE_UI_BROWSER_BROWSER_UI_H_

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class Browser;

class BrowserUI {
 public:
  virtual ~BrowserUI() {}
  static BrowserUI* Create(Browser* browser, mojo::Shell* shell);

  // Called when the Browser UI is embedded within the specified view.
  virtual void Init(mojo::View* root, mojo::View* content) = 0;

};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_UI_H_
