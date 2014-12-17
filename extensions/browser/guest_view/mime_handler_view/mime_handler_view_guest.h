// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_

#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/guest_view/guest_view.h"

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace extensions {

class MimeHandlerViewGuestDelegate;

class MimeHandlerViewGuest : public GuestView<MimeHandlerViewGuest>,
                             public ExtensionFunctionDispatcher::Delegate {
 public:
  static GuestViewBase* Create(content::BrowserContext* browser_context,
                               content::WebContents* owner_web_contents,
                               int guest_instance_id);

  static const char Type[];

  // ExtensionFunctionDispatcher::Delegate implementation.
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // GuestViewBase implementation.
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize() override;
  bool ZoomPropagatesFromEmbedderToGuest() const override;

  // content::BrowserPluginGuestDelegate implementation
  bool Find(int request_id,
            const base::string16& search_text,
            const blink::WebFindOptions& options) override;

  // WebContentsDelegate implementation.
  void ContentsZoomChange(bool zoom_in) override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  bool SaveFrame(const GURL& url, const content::Referrer& referrer) override;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  MimeHandlerViewGuest(content::BrowserContext* browser_context,
                       content::WebContents* owner_web_contents,
                       int guest_instance_id);
  ~MimeHandlerViewGuest() override;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  scoped_ptr<MimeHandlerViewGuestDelegate> delegate_;
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;
  GURL content_url_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
