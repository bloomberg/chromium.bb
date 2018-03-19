// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

namespace extensions {

class ShellWebViewGuestDelegate : public WebViewGuestDelegate {
 public:
  ShellWebViewGuestDelegate();
  ~ShellWebViewGuestDelegate() override;

  // WebViewGuestDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void OnShowContextMenu(int request_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWebViewGuestDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_
