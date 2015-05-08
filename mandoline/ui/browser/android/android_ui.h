// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_
#define MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_

#include "base/macros.h"
#include "mandoline/ui/browser/browser_ui.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class Browser;

class AndroidUI : public BrowserUI {
 public:
  AndroidUI(Browser* browser, mojo::Shell* shell);
  ~AndroidUI() override;

 private:
  // Overridden from BrowserUI:
  void Init(mojo::View* root, mojo::View* content) override;

  Browser* browser_;
  mojo::Shell* shell_;
  mojo::View* root_;
  mojo::View* content_;

  DISALLOW_COPY_AND_ASSIGN(AndroidUI);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_
