// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class Browser;

// Android does not support extensions, so this is just a stub.
class ExtensionViewAndroid {
 public:
  Browser* browser() const { return NULL; }
  gfx::NativeView native_view() const { return NULL; }

  void ResizeDueToAutoResize(const gfx::Size& new_size);
  void RenderViewCreated();

 private:
  ExtensionViewAndroid() { }

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_
