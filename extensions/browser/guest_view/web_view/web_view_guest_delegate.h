// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_

#include "base/callback.h"
#include "components/guest_view/browser/guest_view_base.h"

namespace content {
class RenderViewHost;
class WebContents;
}  // namespace content

namespace extensions {

class WebViewGuest;

namespace api {
namespace web_view_internal {

struct ContextMenuItem;
}  // namespace web_view_internal
}  // namespace api

// A delegate class of WebViewGuest that are not a part of chrome.
class WebViewGuestDelegate {
 public :
  virtual ~WebViewGuestDelegate() {}

  typedef std::vector<linked_ptr<api::web_view_internal::ContextMenuItem> >
      MenuItemVector;

  // Called when context menu operation was handled.
  virtual bool HandleContextMenu(const content::ContextMenuParams& params) = 0;

  // Called just after additional initialization is performed.
  virtual void OnDidInitialize() = 0;

  // Shows the context menu for the guest.
  // |items| acts as a filter. This restricts the current context's default
  // menu items to contain only the items from |items|.
  // |items| == NULL means no filtering will be applied.
  virtual void OnShowContextMenu(
      int request_id,
      const MenuItemVector* items) = 0;

  // Returns true if the WebViewGuest should handle find requests for its
  // embedder.
  virtual bool ShouldHandleFindRequestsForEmbedder() const = 0;

  virtual void SetContextMenuPosition(const gfx::Point& position) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
