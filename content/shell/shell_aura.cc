// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

namespace content {

// static
void Shell::PlatformInitialize() {
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  return base::StringPiece();
}

void Shell::PlatformExit() {
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
}

void Shell::PlatformSetContents() {
}

void Shell::PlatformResizeSubViews() {
}

void Shell::Close() {
}

void Shell::PlatformSetTitle(const string16& title) {
}

}  // namespace content
