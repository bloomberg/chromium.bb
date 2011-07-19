// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_DELEGATE_H_
#pragma once

namespace views {
class NonClientFrameView;
class RootView;
}

class NativeBrowserFrameDelegate {
 public:
  virtual ~NativeBrowserFrameDelegate() {}

  // TODO(beng): Remove these once BrowserFrame is-a Window is-a Widget, at
  //             which point BrowserFrame can just override Widget's method.
  virtual views::RootView* DelegateCreateRootView() = 0;
  virtual views::NonClientFrameView* DelegateCreateFrameViewForWindow() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_DELEGATE_H_
