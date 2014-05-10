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
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_client.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

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
    const std::string& storage_partition_id,
    bool persist_storage,
    scoped_ptr<base::DictionaryValue> extra_params) {
  if (!GetDelegate())
    return NULL;
  WebContents* guest_web_contents =
      GetDelegate()->CreateGuest(embedder_site_instance,
                                 instance_id,
                                 storage_partition_id,
                                 persist_storage,
                                 extra_params.Pass());

  return static_cast<WebContentsImpl*>(guest_web_contents)->
      GetBrowserPluginGuest();
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
