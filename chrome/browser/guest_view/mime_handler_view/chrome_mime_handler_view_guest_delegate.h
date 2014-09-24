// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"

namespace content {
class WebContents;
}  // namespace content

class ChromeMimeHandlerViewGuestDelegate
    : public extensions::MimeHandlerViewGuestDelegate {
 public:
  explicit ChromeMimeHandlerViewGuestDelegate(
      extensions::MimeHandlerViewGuest* guest);
  virtual ~ChromeMimeHandlerViewGuestDelegate();

  // MimeHandlerViewGuestDelegate.
  virtual void AttachHelpers() OVERRIDE;
  virtual void ChangeZoom(bool zoom_in) OVERRIDE;

 private:
  extensions::MimeHandlerViewGuest* guest_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(ChromeMimeHandlerViewGuestDelegate);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
