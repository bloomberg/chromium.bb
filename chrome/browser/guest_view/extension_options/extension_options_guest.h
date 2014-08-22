// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
#define CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_

#include "base/macros.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/guest_view/guest_view.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

class ExtensionOptionsGuest
    : public extensions::GuestView<ExtensionOptionsGuest>,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  static const char Type[];
  static extensions::GuestViewBase* Create(
      content::BrowserContext* browser_context, int guest_instance_id);

  // GuestViewBase implementation.
  virtual const char* GetAPINamespace() OVERRIDE;
  virtual void CreateWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback) OVERRIDE;
  virtual void DidAttachToEmbedder() OVERRIDE;
  virtual void DidInitialize() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void GuestSizeChangedDueToAutoSize(
      const gfx::Size& old_size,
      const gfx::Size& new_size) OVERRIDE;
  virtual bool IsAutoSizeSupported() const OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate implementation.
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // content::WebContentsDelegate implementation.
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const base::string16& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  ExtensionOptionsGuest(content::BrowserContext* browser_context,
                        int guest_instance_id);
  virtual ~ExtensionOptionsGuest();
  void SetUpAutoSize();
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;
  GURL options_page_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionOptionsGuest);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
