// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "content/shell/android/shell_manager.h"

namespace content {

base::StringPiece Shell::PlatformResourceProvider(int key) {
  return base::StringPiece();
}

void Shell::PlatformInitialize() {
}

void Shell::PlatformCleanUp() {
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
}

void Shell::PlatformSetIsLoading(bool loading) {
}

void Shell::PlatformCreateWindow(int width, int height) {
  shell_view_.reset(CreateShellView());
}

void Shell::PlatformSetContents() {
  shell_view_->InitFromTabContents(web_contents_.get());
}

void Shell::PlatformResizeSubViews() {
  // Not needed; subviews are bound.
}

void Shell::PlatformSetTitle(const string16& title) {
  NOTIMPLEMENTED();
}

void Shell::Close() {
  // TODO(tedchoc): Implement Close method for android shell
  NOTIMPLEMENTED();
}

}  // namespace content
