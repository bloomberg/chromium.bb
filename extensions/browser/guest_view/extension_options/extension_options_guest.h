// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_

#include "base/macros.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"
#include "extensions/browser/guest_view/guest_view.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionOptionsGuest
    : public extensions::GuestView<ExtensionOptionsGuest>,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  static const char Type[];
  static extensions::GuestViewBase* Create(
      content::BrowserContext* browser_context,
      content::WebContents* owner_web_contents,
      int guest_instance_id);

  // GuestViewBase implementation.
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize() override;
  void DidStopLoading() override;
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) override;
  bool IsAutoSizeSupported() const override;

  // ExtensionFunctionDispatcher::Delegate implementation.
  content::WebContents* GetAssociatedWebContents() const override;

  // content::WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void CloseContents(content::WebContents* source) override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      int main_frame_route_id,
      WindowContainerType window_container_type,
      const base::string16& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;

  // content::WebContentsObserver implementation.
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ExtensionOptionsGuest(content::BrowserContext* browser_context,
                        content::WebContents* owner_web_contents,
                        int guest_instance_id);
  ~ExtensionOptionsGuest() override;
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void SetUpAutoSize();

  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;
  scoped_ptr<extensions::ExtensionOptionsGuestDelegate>
      extension_options_guest_delegate_;
  GURL options_page_;
  bool has_navigated_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionOptionsGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
