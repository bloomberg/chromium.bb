// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_client.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/escape.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuestManager::factory_ = NULL;

BrowserPluginGuestManager::BrowserPluginGuestManager(BrowserContext* context)
    : context_(context) {}

BrowserPluginGuestManagerDelegate*
BrowserPluginGuestManager::GetDelegate() const {
  return context_->GetGuestManagerDelegate();
}

BrowserPluginGuestManager::~BrowserPluginGuestManager() {
}

// static.
BrowserPluginGuestManager* BrowserPluginGuestManager::FromBrowserContext(
    BrowserContext* context) {
  BrowserPluginGuestManager* guest_manager =
      static_cast<BrowserPluginGuestManager*>(
        context->GetUserData(
            browser_plugin::kBrowserPluginGuestManagerKeyName));
  if (!guest_manager) {
    guest_manager = BrowserPluginGuestManager::Create(context);
    context->SetUserData(browser_plugin::kBrowserPluginGuestManagerKeyName,
                         guest_manager);
  }
  return guest_manager;
}

// static
BrowserPluginGuestManager* BrowserPluginGuestManager::Create(
    BrowserContext* context) {
  if (factory_)
    return factory_->CreateBrowserPluginGuestManager(context);
  return new BrowserPluginGuestManager(context);
}

int BrowserPluginGuestManager::GetNextInstanceID() {
  if (!GetDelegate())
    return 0;
  return GetDelegate()->GetNextInstanceID();
}

BrowserPluginGuest* BrowserPluginGuestManager::CreateGuest(
    SiteInstance* embedder_site_instance,
    int instance_id,
    const BrowserPluginHostMsg_Attach_Params& params,
    scoped_ptr<base::DictionaryValue> extra_params) {
  RenderProcessHost* embedder_process_host =
      embedder_site_instance->GetProcess();
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!IsStringUTF8(params.storage_partition_id)) {
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        embedder_process_host->GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return NULL;
  }

  const GURL& embedder_site_url = embedder_site_instance->GetSiteURL();
  const std::string& host = embedder_site_url.host();

  std::string url_encoded_partition = net::EscapeQueryParamValue(
      params.storage_partition_id, false);
  // The SiteInstance of a given webview tag is based on the fact that it's
  // a guest process in addition to which platform application the tag
  // belongs to and what storage partition is in use, rather than the URL
  // that the tag is being navigated to.
  GURL guest_site(base::StringPrintf("%s://%s/%s?%s",
                                     kGuestScheme,
                                     host.c_str(),
                                     params.persist_storage ? "persist" : "",
                                     url_encoded_partition.c_str()));

  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  SiteInstance* guest_site_instance = GetGuestSiteInstance(guest_site);
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    guest_site_instance = SiteInstance::CreateForURL(
        embedder_site_instance->GetBrowserContext(), guest_site);
  }

  return WebContentsImpl::CreateGuest(
      embedder_site_instance->GetBrowserContext(),
      guest_site_instance,
      instance_id,
      extra_params.Pass());
}

static void BrowserPluginGuestByInstanceIDCallback(
    const BrowserPluginGuestManager::GuestByInstanceIDCallback& callback,
    WebContents* guest_web_contents) {
  if (!guest_web_contents) {
    callback.Run(NULL);
    return;
  }
  callback.Run(static_cast<WebContentsImpl*>(guest_web_contents)->
                   GetBrowserPluginGuest());
}

void BrowserPluginGuestManager::MaybeGetGuestByInstanceIDOrKill(
    int instance_id,
    int embedder_render_process_id,
    const GuestByInstanceIDCallback& callback) const {
  if (!GetDelegate()) {
    callback.Run(NULL);
    return;
  }

  GetDelegate()->MaybeGetGuestByInstanceIDOrKill(
      instance_id,
      embedder_render_process_id,
      base::Bind(&BrowserPluginGuestByInstanceIDCallback,
                 callback));
}

void BrowserPluginGuestManager::AddGuest(int instance_id,
                                         WebContents* guest_web_contents) {
  if (!GetDelegate())
    return;
  GetDelegate()->AddGuest(instance_id, guest_web_contents);
}

void BrowserPluginGuestManager::RemoveGuest(int instance_id) {
  if (!GetDelegate())
    return;
  GetDelegate()->RemoveGuest(instance_id);
}

static void BrowserPluginGuestMessageCallback(const IPC::Message& message,
                                              BrowserPluginGuest* guest) {
  if (!guest)
    return;
  guest->OnMessageReceivedFromEmbedder(message);
}

void BrowserPluginGuestManager::OnMessageReceived(const IPC::Message& message,
                                                  int render_process_id) {
  DCHECK(BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(message));
  int instance_id = 0;
  // All allowed messages must have instance_id as their first parameter.
  PickleIterator iter(message);
  bool success = iter.ReadInt(&instance_id);
  DCHECK(success);
  MaybeGetGuestByInstanceIDOrKill(instance_id,
                                  render_process_id,
                                  base::Bind(&BrowserPluginGuestMessageCallback,
                                             message));
}

SiteInstance* BrowserPluginGuestManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  if (!GetDelegate())
    return NULL;
  return GetDelegate()->GetGuestSiteInstance(guest_site);
}

static bool BrowserPluginGuestCallback(
    const BrowserPluginGuestManager::GuestCallback& callback,
    WebContents* guest_web_contents) {
  return callback.Run(static_cast<WebContentsImpl*>(guest_web_contents)
                          ->GetBrowserPluginGuest());
}

bool BrowserPluginGuestManager::ForEachGuest(
    WebContents* embedder_web_contents, const GuestCallback& callback) {
  if (!GetDelegate())
    return false;
  return GetDelegate()->ForEachGuest(embedder_web_contents,
                                     base::Bind(&BrowserPluginGuestCallback,
                                                callback));
}

}  // namespace content
