// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller_web_contents_delegate.h"

#include "content/public/browser/web_contents.h"

UnloadControllerWebContentsDelegate::UnloadControllerWebContentsDelegate() {}
UnloadControllerWebContentsDelegate::~UnloadControllerWebContentsDelegate() {}

bool UnloadControllerWebContentsDelegate::ShouldSuppressDialogs(
    content::WebContents* source) {
  return true;
}

void UnloadControllerWebContentsDelegate::CloseContents(
    content::WebContents* source) {
  delete source;
}
