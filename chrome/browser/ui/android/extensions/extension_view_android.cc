// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_view_android.h"

#include "base/logging.h"

ExtensionViewAndroid::ExtensionViewAndroid() {
}

ExtensionViewAndroid::~ExtensionViewAndroid() {
}

Browser* ExtensionViewAndroid::GetBrowser() {
  NOTIMPLEMENTED();
  return NULL;
}

const Browser* ExtensionViewAndroid::GetBrowser() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView ExtensionViewAndroid::GetNativeView() {
  NOTIMPLEMENTED();
  return gfx::NativeView();
}

content::RenderViewHost* ExtensionViewAndroid::GetRenderViewHost() const {
  NOTIMPLEMENTED();
  return NULL;
}

void ExtensionViewAndroid::SetContainer(ExtensionViewContainer* container) {
  NOTIMPLEMENTED();
}

void ExtensionViewAndroid::ResizeDueToAutoResize(const gfx::Size& new_size) {
  NOTIMPLEMENTED();
}

void ExtensionViewAndroid::RenderViewCreated() {
  NOTIMPLEMENTED();
}

void ExtensionViewAndroid::DidStopLoading() {
  NOTIMPLEMENTED();
}

void ExtensionViewAndroid::WindowFrameChanged() {
  NOTIMPLEMENTED();
}

// static
ExtensionView* ExtensionView::Create(extensions::ExtensionHost* host,
                                     Browser* browser) {
  return new ExtensionViewAndroid();
}
