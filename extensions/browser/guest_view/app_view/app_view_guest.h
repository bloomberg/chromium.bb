// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_

#include "base/id_map.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/guest_view/app_view/app_view_guest_delegate.h"
#include "extensions/browser/guest_view/guest_view.h"

namespace extensions {
class Extension;
class ExtensionHost;

// An AppViewGuest provides the browser-side implementation of <appview> API.
// AppViewGuest is created on attachment. That is, when a guest WebContents is
// associated with a particular embedder WebContents. This happens on calls to
// the connect API.
class AppViewGuest : public GuestView<AppViewGuest>,
                     public ExtensionFunctionDispatcher::Delegate {
 public:
  static const char Type[];

  // Completes the creation of a WebContents associated with the provided
  // |guest_extensions_id|.
  static bool CompletePendingRequest(
      content::BrowserContext* browser_context,
      const GURL& url,
      int guest_instance_id,
      const std::string& guest_extension_id);

  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  // ExtensionFunctionDispatcher::Delegate implementation.
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // content::WebContentsDelegate implementation.
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;

  // GuestViewBase implementation.
  bool CanRunInDetachedState() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidInitialize(const base::DictionaryValue& create_params) override;
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;

  // Sets the AppDelegate for this guest.
  void SetAppDelegateForTest(AppDelegate* delegate);

 private:
  explicit AppViewGuest(content::WebContents* owner_web_contents);

  ~AppViewGuest() override;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  void CompleteCreateWebContents(const GURL& url,
                                 const Extension* guest_extension,
                                 const WebContentsCreatedCallback& callback);

  void LaunchAppAndFireEvent(scoped_ptr<base::DictionaryValue> data,
                             const WebContentsCreatedCallback& callback,
                             ExtensionHost* extension_host);

  GURL url_;
  std::string guest_extension_id_;
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;
  scoped_ptr<AppViewGuestDelegate> app_view_guest_delegate_;
  scoped_ptr<AppDelegate> app_delegate_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<AppViewGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
