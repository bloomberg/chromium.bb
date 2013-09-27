// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_PLUGIN_PERMISSION_HELPER_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_PLUGIN_PERMISSION_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PluginPermissionHelper
    : public content::WebContentsUserData<PluginPermissionHelper>,
      public content::WebContentsObserver {
 public:
  virtual ~PluginPermissionHelper();

 private:
  explicit PluginPermissionHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PluginPermissionHelper>;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers:
  void OnBlockedUnauthorizedPlugin(const string16& name,
                                   const std::string& identifier);
  void OnCouldNotLoadPlugin(const base::FilePath& plugin_path);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
  void OnNPAPINotSupported(const std::string& identifier);
  void OnOpenAboutPlugins();
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void OnRemovePluginPlaceholderHost(int placeholder_id);
#endif

  void OnPermissionResponse(const std::string& identifier,
                            bool allow,
                            const std::string& user_input);

  base::WeakPtrFactory<PluginPermissionHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginPermissionHelper);
};

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_PLUGIN_PERMISSION_HELPER_H_
