// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include "base/macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class MimeHandlerViewGuest;

// A delegate class of MimeHandlerViewGuest that are not a part of chrome.
class MimeHandlerViewGuestDelegate {
 public:
  explicit MimeHandlerViewGuestDelegate(MimeHandlerViewGuest* guest) {}
  virtual ~MimeHandlerViewGuestDelegate() {}

  // Attaches helpers upon initializing the WebContents.
  virtual void AttachHelpers() {}

  // Request to change the zoom level of the top level page containing
  // this view.
  virtual void ChangeZoom(bool zoom_in) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuestDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
