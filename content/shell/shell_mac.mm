// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/logging.h"
#include "base/string_piece.h"

namespace content {

void Shell::PlatformInitialize() {
  NOTIMPLEMENTED();
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  NOTIMPLEMENTED();
  return base::StringPiece();
}

void Shell::PlatformCleanUp() {
  NOTIMPLEMENTED();
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  NOTIMPLEMENTED();
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
}

void Shell::PlatformCreateWindow() {
  NOTIMPLEMENTED();
}

void Shell::PlatformSizeTo(int width, int height) {
  NOTIMPLEMENTED();
}

void Shell::PlatformResizeSubViews() {
  NOTIMPLEMENTED();
}

}  // namespace content
