// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"

namespace content {

class ShellWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  explicit ShellWebContentsViewDelegate(WebContents* web_contents);
  virtual ~ShellWebContentsViewDelegate();

  // Overridden from WebContentsViewDelegate:
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) OVERRIDE;

#if defined(OS_MACOSX)
  void ActionPerformed(int id);
#elif defined(OS_WIN)
  void MenuItemSelected(int selection);
#endif

#if defined(TOOLKIT_VIEWS)
  virtual void ShowDisambiguationPopup(
      const gfx::Rect& target_rect,
      const SkBitmap& zoomed_bitmap,
      const gfx::NativeView content,
      const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
      const base::Callback<void(ui::MouseEvent*)>& mouse_cb) OVERRIDE;

  virtual void HideDisambiguationPopup() OVERRIDE;
#endif

 private:
  WebContents* web_contents_;
  ContextMenuParams params_;

  DISALLOW_COPY_AND_ASSIGN(ShellWebContentsViewDelegate);
};

}  // namespace content

#endif // CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
