// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"

using content::ChildProcessSecurityPolicy;
using content::RenderViewHost;
using content::SiteInstance;
using extensions::Extension;

ChromeRenderViewHostObserver::ChromeRenderViewHostObserver(
    RenderViewHost* render_view_host, chrome_browser_net::Predictor* predictor)
    : content::RenderViewHostObserver(render_view_host),
      predictor_(predictor) {
  SiteInstance* site_instance = render_view_host->GetSiteInstance();
  profile_ = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());

  InitRenderViewHostForExtensions();
}

ChromeRenderViewHostObserver::~ChromeRenderViewHostObserver() {
  if (render_view_host())
    RemoveRenderViewHostForExtensions(render_view_host());
}

void ChromeRenderViewHostObserver::RenderViewHostInitialized() {
  InitRenderViewForExtensions();
}

void ChromeRenderViewHostObserver::RenderViewHostDestroyed(
    RenderViewHost* rvh) {
  RemoveRenderViewHostForExtensions(rvh);
  delete this;
}

void ChromeRenderViewHostObserver::Navigate(const GURL& url) {
  if (!predictor_)
    return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame) &&
     (url.SchemeIs(chrome::kHttpScheme) || url.SchemeIs(chrome::kHttpsScheme)))
    predictor_->PreconnectUrlAndSubresources(url);
}

bool ChromeRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FocusedEditableNodeTouched,
                        OnFocusedEditableNodeTouched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderViewHostObserver::InitRenderViewHostForExtensions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  ExtensionProcessManager* process_manager =
      profile_->GetExtensionProcessManager();
  CHECK(process_manager);

  // TODO(creis): Use this to replace SetInstalledAppForRenderer.
  process_manager->RegisterRenderViewHost(render_view_host(), extension);

  if (extension->is_app()) {
    // Record which, if any, installed app is associated with this process.
    // TODO(aa): Totally lame to store this state in a global map in extension
    // service. Can we get it from EPM instead?
    profile_->GetExtensionService()->SetInstalledAppForRenderer(
        render_view_host()->GetProcess()->GetID(), extension);
  }
}

void ChromeRenderViewHostObserver::InitRenderViewForExtensions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  content::RenderProcessHost* process = render_view_host()->GetProcess();

  if (extension->is_app()) {
    // Though we already record the associated process ID for the renderer in
    // InitRenderViewHostForExtensions, the process might have crashed and been
    // restarted (hence the re-initialization), so we need to update that
    // mapping.
    profile_->GetExtensionService()->SetInstalledAppForRenderer(
        process->GetID(), extension);
  }

  // Some extensions use chrome:// URLs.
  Extension::Type type = extension->GetType();
  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_PACKAGED_APP) {
    ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process->GetID(), chrome::kChromeUIScheme);

    if (profile_->GetExtensionService()->extension_prefs()->AllowFileAccess(
          extension->id())) {
      ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
          process->GetID(), chrome::kFileScheme);
    }
  }

  switch (type) {
    case Extension::TYPE_EXTENSION:
    case Extension::TYPE_USER_SCRIPT:
    case Extension::TYPE_HOSTED_APP:
    case Extension::TYPE_PACKAGED_APP:
    case Extension::TYPE_PLATFORM_APP:
      Send(new ExtensionMsg_ActivateExtension(extension->id()));
      break;

    case Extension::TYPE_UNKNOWN:
    case Extension::TYPE_THEME:
      break;
  }
}

const Extension* ChromeRenderViewHostObserver::GetExtension() {
  // Note that due to ChromeContentBrowserClient::GetEffectiveURL(), hosted apps
  // (excluding bookmark apps) will have a chrome-extension:// URL for their
  // site, so we can ignore that wrinkle here.
  SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  const GURL& site = site_instance->GetSite();

  if (!site.SchemeIs(chrome::kExtensionScheme))
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

void ChromeRenderViewHostObserver::RemoveRenderViewHostForExtensions(
    RenderViewHost* rvh) {
  ExtensionProcessManager* process_manager =
      profile_->GetExtensionProcessManager();
  if (process_manager)
    process_manager->UnregisterRenderViewHost(rvh);
}

void ChromeRenderViewHostObserver::OnFocusedEditableNodeTouched() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOCUSED_EDITABLE_NODE_TOUCHED,
      content::Source<RenderViewHost>(render_view_host()),
      content::NotificationService::NoDetails());
}
