// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_DELEGATE_H_
#pragma once

class TabContents;
namespace gfx{
class Size;
}

namespace internal {

class NativeTabContentsViewDelegate {
 public:
  virtual ~NativeTabContentsViewDelegate() {}

  virtual TabContents* GetTabContents() = 0;

  // TODO(beng):
  // This can die with OnNativeTabContentsViewMouseDown/Move().
  virtual bool IsShowingSadTab() const = 0;

  // TODO(beng):
  // These three can be replaced by Widget::OnSizeChanged and some new
  // notifications for show/hide.
  virtual void OnNativeTabContentsViewShown() = 0;
  virtual void OnNativeTabContentsViewHidden() = 0;
  virtual void OnNativeTabContentsViewSized(const gfx::Size& size) = 0;

  virtual void OnNativeTabContentsViewWheelZoom(int distance) = 0;

  // TODO(beng):
  // These two can be replaced by an override of Widget::OnMouseEvent.
  virtual void OnNativeTabContentsViewMouseDown() = 0;
  virtual void OnNativeTabContentsViewMouseMove() = 0;

  virtual void OnNativeTabContentsViewDraggingEnded() = 0;
};

}  // namespace internal

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_DELEGATE_H_
