// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/supports_user_data.h"

struct HostID;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class UserScript;

// WebViewContentScriptManager manages the content scripts that each webview
// guest adds and removes programmatically.
// TODO(hanxi): crbug.com/476938. Introduce a new class to manage the lifetime
// of <webview> and clean up WebViewContentScriptManager.
class WebViewContentScriptManager : public base::SupportsUserData::Data {
 public:
  explicit WebViewContentScriptManager(
      content::BrowserContext* browser_context);
  ~WebViewContentScriptManager() override;

  static WebViewContentScriptManager* Get(
      content::BrowserContext* browser_context);

  // Adds content scripts for the guest specified by the |embedder_web_contents,
  // view_instance_id|.
  void AddContentScripts(content::WebContents* embedder_web_contents,
                         int view_instance_id,
                         const HostID& host_id,
                         const std::set<UserScript>& user_scripts);

  // Removes contents scipts whose names are in the |script_name_list| for the
  // guest specified by |embedder_web_contents, view_instance_id|.
  // If the |script_name_list| is empty, removes all the content scripts added
  // for this guest.
  void RemoveContentScripts(content::WebContents* embedder_web_contents,
                            int view_instance_id,
                            const HostID& host_id,
                            const std::vector<std::string>& script_name_list);

  // Returns the content script IDs added by the guest specified by
  // |embedder_process_id, view_instance_id|.
  std::set<int> GetContentScriptIDSet(int embedder_process_id,
                                      int view_instance_id);

 private:
  class OwnerWebContentsObserver;

  using GuestMapKey = std::pair<int, int>;
  using ContentScriptMap = std::map<std::string, extensions::UserScript>;
  using GuestContentScriptMap = std::map<GuestMapKey, ContentScriptMap>;

  using OwnerWebContentsObserverMap =
      std::map<content::WebContents*, linked_ptr<OwnerWebContentsObserver>>;

  // Called by OwnerWebContentsObserver when the observer sees a main frame
  // navigation or sees the process has went away or the |embedder_web_contents|
  // is being destroyed.
  void RemoveObserver(content::WebContents* embedder_web_contents);

  OwnerWebContentsObserverMap owner_web_contents_observer_map_;

  GuestContentScriptMap guest_content_script_map_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebViewContentScriptManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H
