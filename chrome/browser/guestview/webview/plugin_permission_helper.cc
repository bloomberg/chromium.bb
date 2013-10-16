// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/plugin_permission_helper.h"

#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/guestview/webview/webview_permission_types.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"

using content::BrowserPluginGuestDelegate;
using content::RenderViewHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PluginPermissionHelper);

PluginPermissionHelper::PluginPermissionHelper(WebContents* contents)
    : content::WebContentsObserver(contents),
      weak_factory_(this) {
}

PluginPermissionHelper::~PluginPermissionHelper() {
}

bool PluginPermissionHelper::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginPermissionHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                        OnBlockedUnauthorizedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CouldNotLoadPlugin,
                        OnCouldNotLoadPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NPAPINotSupported,
                        OnNPAPINotSupported)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenAboutPlugins,
                        OnOpenAboutPlugins)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FindMissingPlugin,
                        OnFindMissingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                        OnRemovePluginPlaceholderHost)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginPermissionHelper::OnBlockedUnauthorizedPlugin(
    const string16& name,
    const std::string& identifier) {
  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  WebViewGuest* webview = WebViewGuest::FromWebContents(web_contents());
  if (!webview)
    return;

  base::DictionaryValue info;
  info.SetString(std::string(kPluginName), name);
  info.SetString(std::string(kPluginIdentifier), identifier);
  webview->RequestPermission(static_cast<BrowserPluginPermissionType>(
          WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN),
      info,
      base::Bind(&PluginPermissionHelper::OnPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 identifier),
      true /* allowed_by_default */);
  content::RecordAction(
      content::UserMetricsAction("WebView.Guest.PluginLoadRequest"));
}

void PluginPermissionHelper::OnCouldNotLoadPlugin(
    const base::FilePath& plugin_path) {
}

void PluginPermissionHelper::OnBlockedOutdatedPlugin(
    int placeholder_id,
    const std::string& identifier) {
}

void PluginPermissionHelper::OnNPAPINotSupported(const std::string& id) {
}

void PluginPermissionHelper::OnOpenAboutPlugins() {
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void PluginPermissionHelper::OnFindMissingPlugin(int placeholder_id,
                                                 const std::string& mime_type) {
}

void PluginPermissionHelper::OnRemovePluginPlaceholderHost(int placeholder_id) {
}
#endif // defined(ENABLE_PLUGIN_INSTALLATION)

void PluginPermissionHelper::OnPermissionResponse(const std::string& identifier,
                                                  bool allow,
                                                  const std::string& input) {
  if (allow) {
    RenderViewHost* host = web_contents()->GetRenderViewHost();
    ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        host->GetProcess()->GetID());
    host->Send(new ChromeViewMsg_LoadBlockedPlugins(
        host->GetRoutingID(), identifier));
  }

  content::RecordAction(
      allow ? content::UserMetricsAction("WebView.Guest.PluginLoadAllowed") :
              content::UserMetricsAction("WebView.Guest.PluginLoadDenied"));
}
