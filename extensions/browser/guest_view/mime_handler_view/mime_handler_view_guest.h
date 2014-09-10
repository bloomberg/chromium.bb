// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_

#include "extensions/browser/guest_view/guest_view.h"

namespace extensions {

class MimeHandlerViewGuestDelegate;

class MimeHandlerViewGuest : public GuestView<MimeHandlerViewGuest> {
 public:
  static GuestViewBase* Create(content::BrowserContext* browser_context,
                               int guest_instance_id);

  static const char Type[];

  // GuestViewBase implementation.
  virtual const char* GetAPINamespace() const OVERRIDE;
  virtual int GetTaskPrefix() const OVERRIDE;
  virtual void CreateWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback) OVERRIDE;
  virtual void DidAttachToEmbedder() OVERRIDE;
  virtual void DidInitialize() OVERRIDE;

  // WebContentsDelegate implementation.
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

 private:
  MimeHandlerViewGuest(content::BrowserContext* browser_context,
                       int guest_instance_id);
  virtual ~MimeHandlerViewGuest();

  scoped_ptr<MimeHandlerViewGuestDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
