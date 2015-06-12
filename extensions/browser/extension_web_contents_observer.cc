// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_web_contents_observer.h"

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/mojo/service_registration.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"

namespace extensions {
namespace {

const Extension* GetExtensionForRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  GURL url = render_frame_host->GetLastCommittedURL();
  if (!url.is_empty()) {
    if (site_instance->GetSiteURL().GetOrigin() != url.GetOrigin())
      return nullptr;
  } else {
    url = site_instance->GetSiteURL();
  }
  content::BrowserContext* browser_context = site_instance->GetBrowserContext();
  if (!url.SchemeIs(kExtensionScheme))
    return nullptr;

  return ExtensionRegistry::Get(browser_context)
      ->enabled_extensions()
      .GetExtensionOrAppByURL(url);
}

}  // namespace

// static
ExtensionWebContentsObserver* ExtensionWebContentsObserver::GetForWebContents(
    content::WebContents* web_contents) {
  return ExtensionsBrowserClient::Get()->GetExtensionWebContentsObserver(
      web_contents);
}

ExtensionWebContentsObserver::ExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      dispatcher_(browser_context_) {
  NotifyRenderViewType(web_contents->GetRenderViewHost());
  content::RenderFrameHost* host = web_contents->GetMainFrame();
  if (host)
    RenderFrameHostChanged(nullptr, host);
  dispatcher_.set_delegate(this);
}

ExtensionWebContentsObserver::~ExtensionWebContentsObserver() {
}

content::WebContents* ExtensionWebContentsObserver::GetAssociatedWebContents()
    const {
  return web_contents();
}

void ExtensionWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  NotifyRenderViewType(render_view_host);

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
        process->GetID(), content::kChromeUIScheme);
  }

  // Some extensions use file:// URLs.
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
    if (prefs->AllowFileAccess(extension->id())) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
          process->GetID(), url::kFileScheme);
    }
  }

  // Tells the new view that it's hosted in an extension process.
  //
  // This will often be a rendant IPC, because activating extensions happens at
  // the process level, not at the view level. However, without some mild
  // refactoring this isn't trivial to do, and this way is simpler.
  //
  // Plus, we can delete the concept of activating an extension once site
  // isolation is turned on.
  render_view_host->Send(new ExtensionMsg_ActivateExtension(extension->id()));
}

void ExtensionWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  const Extension* extension = GetExtensionForRenderFrame(render_frame_host);
  if (extension) {
    ExtensionsBrowserClient::Get()->RegisterMojoServices(render_frame_host,
                                                         extension);
  }
}

bool ExtensionWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(
      ExtensionWebContentsObserver, message, render_frame_host)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionWebContentsObserver::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  ProcessManager::Get(browser_context_)->UnregisterRenderFrameHost(
      render_frame_host);
}

void ExtensionWebContentsObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  if (old_host)
    process_manager->UnregisterRenderFrameHost(old_host);

  const Extension* frame_extension = GetExtensionForRenderFrame(new_host);
  if (frame_extension) {
    process_manager->RegisterRenderFrameHost(
        web_contents(), new_host, frame_extension);
  }

  // This can be different from |frame_extension| above in the case of, e.g.,
  // a non-extension iframe hosted in a chrome-extension:// page.
  const Extension* tab_extension =
      GetExtensionForRenderFrame(web_contents()->GetMainFrame());
  if (tab_extension) {
    new_host->Send(new ExtensionMsg_SetTabExtensionOwner(
        new_host->GetRoutingID(), tab_extension->id()));
  }
}

void ExtensionWebContentsObserver::NotifyRenderViewType(
    content::RenderViewHost* render_view_host) {
  if (render_view_host) {
    render_view_host->Send(new ExtensionMsg_NotifyRenderViewType(
        render_view_host->GetRoutingID(), GetViewType(web_contents())));
  }
}

void ExtensionWebContentsObserver::PepperInstanceCreated() {
  ProcessManager* const process_manager = ProcessManager::Get(browser_context_);
  const Extension* const extension =
      process_manager->GetExtensionForWebContents(web_contents());
  if (extension)
    process_manager->IncrementLazyKeepaliveCount(extension);
}

void ExtensionWebContentsObserver::PepperInstanceDeleted() {
  ProcessManager* const process_manager = ProcessManager::Get(browser_context_);
  const Extension* const extension =
      process_manager->GetExtensionForWebContents(web_contents());
  if (extension)
    process_manager->DecrementLazyKeepaliveCount(extension);
}

const Extension* ExtensionWebContentsObserver::GetExtension(
    content::RenderViewHost* render_view_host) {
  std::string extension_id = GetExtensionId(render_view_host);
  if (extension_id.empty())
    return NULL;

  // May be null if the extension doesn't exist, for example if somebody typos
  // a chrome-extension:// URL.
  return ExtensionRegistry::Get(browser_context_)
      ->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
}

// static
std::string ExtensionWebContentsObserver::GetExtensionId(
    content::RenderViewHost* render_view_host) {
  // Note that due to ChromeContentBrowserClient::GetEffectiveURL(), hosted apps
  // (excluding bookmark apps) will have a chrome-extension:// URL for their
  // site, so we can ignore that wrinkle here.
  const GURL& site = render_view_host->GetSiteInstance()->GetSiteURL();

  if (!site.SchemeIs(kExtensionScheme))
    return std::string();

  return site.host();
}

void ExtensionWebContentsObserver::OnRequest(
    content::RenderFrameHost* render_frame_host,
    const ExtensionHostMsg_Request_Params& params) {
  dispatcher_.Dispatch(params, render_frame_host);
}

}  // namespace extensions
