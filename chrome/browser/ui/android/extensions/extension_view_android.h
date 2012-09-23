// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_view.h"

class ExtensionViewAndroid : public ExtensionView {
 public:
  ExtensionViewAndroid();
  virtual ~ExtensionViewAndroid();

 private:
  // Overridden from ExtensionView:
  virtual Browser* GetBrowser() OVERRIDE;
  virtual const Browser* GetBrowser() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual void SetContainer(ExtensionViewContainer* container) OVERRIDE;
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void RenderViewCreated() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_VIEW_ANDROID_H_
