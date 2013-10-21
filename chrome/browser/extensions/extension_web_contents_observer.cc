// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_contents_observer.h"

#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::ExtensionWebContentsObserver);

namespace extensions {

ExtensionWebContentsObserver::ExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
}

ExtensionWebContentsObserver::~ExtensionWebContentsObserver() {
}

void ExtensionWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->Send(new ExtensionMsg_NotifyRenderViewType(
      render_view_host->GetRoutingID(),
      extensions::GetViewType(web_contents())));

  const Extension* extension = GetExtension(render_view_host);
  if (!extension)
    return;

  content::RenderProcessHost* process = render_view_host->GetProcess();

  // Some extensions use chrome:// URLs.
  // This is a temporary solution. Replace it with access to chrome-static://
  // once it is implemented. See: crbug.com/226927.
  Manifest::Type type = extension->GetType();
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP ||
      (type == Manifest::TYPE_PLATFORM_APP &&
       extension->location() == Manifest::COMPONENT)) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process->GetID(), chrome::kChromeUIScheme);
  }

  // Some extensions use file:// URLs.
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
    if (ExtensionSystem::Get(profile_)->extension_service()->
            extension_prefs()->AllowFileAccess(extension->id())) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
          process->GetID(), chrome::kFileScheme);
    }
  }

  switch (type) {
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_USER_SCRIPT:
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
      // Always send a Loaded message before ActivateExtension so that
      // ExtensionDispatcher knows what Extension is active, not just its ID.
      // This is important for classifying the Extension's JavaScript context
      // correctly (see ExtensionDispatcher::ClassifyJavaScriptContext).
      render_view_host->Send(new ExtensionMsg_Loaded(
          std::vector<ExtensionMsg_Loaded_Params>(
              1, ExtensionMsg_Loaded_Params(extension))));
      render_view_host->Send(
          new ExtensionMsg_ActivateExtension(extension->id()));
      break;

    case Manifest::TYPE_UNKNOWN:
    case Manifest::TYPE_THEME:
    case Manifest::TYPE_SHARED_MODULE:
      break;
  }
}

bool ExtensionWebContentsObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionWebContentsObserver, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionWebContentsObserver::OnPostMessage(int port_id,
                                                 const std::string& message) {
  MessageService* message_service = MessageService::Get(profile_);
  if (message_service) {
    message_service->PostMessage(port_id, message);
  }
}

const Extension* ExtensionWebContentsObserver::GetExtension(
    content::RenderViewHost* render_view_host) {
  // Note that due to ChromeContentBrowserClient::GetEffectiveURL(), hosted apps
  // (excluding bookmark apps) will have a chrome-extension:// URL for their
  // site, so we can ignore that wrinkle here.
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  const GURL& site = site_instance->GetSiteURL();

  if (!site.SchemeIs(kExtensionScheme))
    return NULL;

  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return NULL;

  // Reload the extension if it has crashed.
  // TODO(yoz): This reload doesn't happen synchronously for unpacked
  //            extensions. It seems to be fast enough, but there is a race.
  //            We should delay loading until the extension has reloaded.
  if (service->GetTerminatedExtension(site.host()))
    service->ReloadExtension(site.host());

  // May be null if the extension doesn't exist, for example if somebody typos
  // a chrome-extension:// URL.
  return service->extensions()->GetByID(site.host());
}

}  // namespace extensions
