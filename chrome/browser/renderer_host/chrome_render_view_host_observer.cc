// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"

#include "base/command_line.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/common/bindings_policy.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"

ChromeRenderViewHostObserver::ChromeRenderViewHostObserver(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
  InitRenderViewHostForExtensions();
}

ChromeRenderViewHostObserver::~ChromeRenderViewHostObserver() {
}

void ChromeRenderViewHostObserver::RenderViewHostInitialized() {
  InitRenderViewForExtensions();
}

void ChromeRenderViewHostObserver::Navigate(
    const ViewMsg_Navigate_Params& params) {
  const GURL& url = params.url;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame) &&
      (url.SchemeIs(chrome::kHttpScheme) || url.SchemeIs(chrome::kHttpsScheme)))
    chrome_browser_net::PreconnectUrlAndSubresources(url);
}

bool ChromeRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderViewHostObserver::InitRenderViewHostForExtensions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  SiteInstance* site_instance = render_view_host()->site_instance();
  Profile* profile = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());
  ExtensionProcessManager* process_manager =
      profile->GetExtensionProcessManager();
  CHECK(process_manager);

  site_instance->GetProcess()->mark_is_extension_process();

  // Register the association between extension and SiteInstance with
  // ExtensionProcessManager.
  // TODO(creis): Use this to replace SetInstalledAppForRenderer.
  process_manager->RegisterExtensionSiteInstance(site_instance->id(),
                                                 extension->id());

  if (extension->is_app()) {
    // Record which, if any, installed app is associated with this process.
    // TODO(aa): Totally lame to store this state in a global map in extension
    // service. Can we get it from EPM instead?
    profile->GetExtensionService()->SetInstalledAppForRenderer(
        render_view_host()->process()->id(), extension);
  }

  // Enable extension bindings for the renderer. Currently only extensions,
  // packaged apps, and hosted component apps use extension bindings.
  Extension::Type type = extension->GetType();
  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_USER_SCRIPT ||
      type == Extension::TYPE_PACKAGED_APP ||
      (type == Extension::TYPE_HOSTED_APP &&
       extension->location() == Extension::COMPONENT)) {
    render_view_host()->AllowBindings(BindingsPolicy::EXTENSION);
    ChildProcessSecurityPolicy::GetInstance()->GrantExtensionBindings(
        render_view_host()->process()->id());
  }
}

void ChromeRenderViewHostObserver::InitRenderViewForExtensions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  SiteInstance* site_instance = render_view_host()->site_instance();
  Profile* profile = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());
  RenderProcessHost* process = render_view_host()->process();

  if (extension->is_app()) {
    Send(new ExtensionMsg_ActivateApplication(extension->id()));
    // Though we already record the associated process ID for the renderer in
    // InitRenderViewHostForExtensions, the process might have crashed and been
    // restarted (hence the re-initialization), so we need to update that
    // mapping.
    profile->GetExtensionService()->SetInstalledAppForRenderer(
        process->id(), extension);
  }

  // Some extensions use chrome:// URLs.
  Extension::Type type = extension->GetType();
  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_PACKAGED_APP) {
    ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process->id(), chrome::kChromeUIScheme);
  }

  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_USER_SCRIPT ||
      type == Extension::TYPE_PACKAGED_APP ||
      (type == Extension::TYPE_HOSTED_APP &&
       extension->location() == Extension::COMPONENT)) {
    Send(new ExtensionMsg_ActivateExtension(extension->id()));
  }
}

const Extension* ChromeRenderViewHostObserver::GetExtension() {
  // Note that due to ChromeContentBrowserClient::GetEffectiveURL(), even hosted
  // apps will have a chrome-extension:// URL for their site, so we can ignore
  // that wrinkle here.
  SiteInstance* site_instance = render_view_host()->site_instance();
  const GURL& site = site_instance->site();

  if (!site.SchemeIs(chrome::kExtensionScheme))
    return NULL;

  Profile* profile = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return NULL;

  // May be null if somebody typos a chrome-extension:// URL.
  return service->GetExtensionByURL(site);
}

void ChromeRenderViewHostObserver::OnDomOperationResponse(
    const std::string& json_string, int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_DOM_OPERATION_RESPONSE,
      Source<RenderViewHost>(render_view_host()),
      Details<DomOperationNotificationDetails>(&details));
}
