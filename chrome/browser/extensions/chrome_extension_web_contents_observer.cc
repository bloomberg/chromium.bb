// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"

#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"

using content::BrowserContext;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    extensions::ChromeExtensionWebContentsObserver);

namespace extensions {

ChromeExtensionWebContentsObserver::ChromeExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents) {}

ChromeExtensionWebContentsObserver::~ChromeExtensionWebContentsObserver() {}

void ChromeExtensionWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  ReloadIfTerminated(render_view_host);
  ExtensionWebContentsObserver::RenderViewCreated(render_view_host);
}

bool ChromeExtensionWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeExtensionWebContentsObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DetailedConsoleMessageAdded,
                        OnDetailedConsoleMessageAdded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeExtensionWebContentsObserver::OnDetailedConsoleMessageAdded(
    const base::string16& message,
    const base::string16& source,
    const StackTrace& stack_trace,
    int32 severity_level) {
  if (!IsSourceFromAnExtension(source))
    return;

  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();
  std::string extension_id = GetExtensionId(render_view_host);
  if (extension_id.empty())
    extension_id = GURL(source).host();

  ExtensionSystem::Get(browser_context())->error_console()->ReportError(
      scoped_ptr<ExtensionError>(
          new RuntimeError(extension_id,
                           browser_context()->IsOffTheRecord(),
                           source,
                           message,
                           stack_trace,
                           web_contents()->GetLastCommittedURL(),
                           static_cast<logging::LogSeverity>(severity_level),
                           render_view_host->GetRoutingID(),
                           render_view_host->GetProcess()->GetID())));
}

void ChromeExtensionWebContentsObserver::ReloadIfTerminated(
    content::RenderViewHost* render_view_host) {
  std::string extension_id = GetExtensionId(render_view_host);
  if (extension_id.empty())
    return;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Reload the extension if it has crashed.
  // TODO(yoz): This reload doesn't happen synchronously for unpacked
  //            extensions. It seems to be fast enough, but there is a race.
  //            We should delay loading until the extension has reloaded.
  if (registry->GetExtensionById(extension_id, ExtensionRegistry::TERMINATED)) {
    ExtensionSystem::Get(browser_context())->
        extension_service()->ReloadExtension(extension_id);
  }
}

}  // namespace extensions
