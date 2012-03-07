// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
#pragma once

#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace content {

class WebDragDestDelegate;
struct ContextMenuParams;

// This interface allows a client to extend the functionality of
// WebContentsViewWin.
class CONTENT_EXPORT WebContentsViewWinDelegate {
 public:
  virtual ~WebContentsViewWinDelegate() {}

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* GetDragDestDelegate() = 0;

  // These methods allow the embedder to intercept WebContentsViewWin's
  // implementation of these WebContentsView methods. See the WebContentsView
  // interface documentation for more information about these methods.
  virtual void StoreFocus() = 0;
  virtual void RestoreFocus() = 0;
  virtual bool Focus() = 0;
  virtual void TakeFocus(bool reverse) = 0;
  virtual void ShowContextMenu(const content::ContextMenuParams& params) = 0;
  virtual void SizeChanged(const gfx::Size& size) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_WIN_DELEGATE_H_
