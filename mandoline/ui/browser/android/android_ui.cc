// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/android/android_ui.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class Browser;

AndroidUI::AndroidUI(Browser* browser, mojo::Shell* shell)
    : browser_(browser),
      shell_(shell),
      root_(nullptr),
      content_(nullptr) {}
AndroidUI::~AndroidUI() {}

void AndroidUI::Init(mojo::View* root, mojo::View* content_) {
  root_ = root;
  content_ = content;

  content_->SetBounds(root_->bounds());
}

// static
BrowserUI* BrowserUI::Create(Browser* browser, mojo::Shell* shell) {
  return new AndroidUI(browser, shell);
}

}  // namespace mandoline
