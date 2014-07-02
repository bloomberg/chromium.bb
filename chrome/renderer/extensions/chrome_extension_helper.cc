// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extension_helper.h"

#include "base/strings/string16.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/renderer/web_apps.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

ChromeExtensionHelper::ChromeExtensionHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<ChromeExtensionHelper>(render_view) {
}

ChromeExtensionHelper::~ChromeExtensionHelper() {
}

bool ChromeExtensionHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeExtensionHelper, message)
    IPC_MESSAGE_HANDLER(ChromeExtensionMsg_GetApplicationInfo,
                        OnGetApplicationInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeExtensionHelper::OnGetApplicationInfo() {
  WebApplicationInfo app_info;
  base::string16 error;
  web_apps::ParseWebAppFromWebDocument(
      render_view()->GetWebView()->mainFrame(), &app_info, &error);

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    if (app_info.icons[i].url.SchemeIs(url::kDataScheme)) {
      app_info.icons.erase(app_info.icons.begin() + i);
      --i;
    }
  }

  Send(
      new ChromeExtensionHostMsg_DidGetApplicationInfo(routing_id(), app_info));
}

}  // namespace extensions
