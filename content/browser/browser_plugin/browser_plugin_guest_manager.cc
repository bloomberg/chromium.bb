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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/escape.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuestManager::factory_ = NULL;

BrowserPluginGuestManager::BrowserPluginGuestManager()
    : next_instance_id_(browser_plugin::kInstanceIDNone) {
}

BrowserPluginGuestManager::~BrowserPluginGuestManager() {
}

// static
BrowserPluginGuestManager* BrowserPluginGuestManager::Create() {
  if (factory_)
    return factory_->CreateBrowserPluginGuestManager();
  return new BrowserPluginGuestManager();
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

  // We usually require BrowserPlugins to be hosted by a storage isolated
  // extension. We treat WebUI pages as a special case if they host the
  // BrowserPlugin in a component extension iframe. In that case, we use the
  // iframe's URL to determine the extension.
  const GURL& embedder_site_url = embedder_site_instance->GetSiteURL();
  GURL validated_frame_url(params.embedder_frame_url);
  embedder_process_host->FilterURL(false, &validated_frame_url);
  const std::string& host = content::HasWebUIScheme(embedder_site_url) ?
       validated_frame_url.host() : embedder_site_url.host();

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

BrowserPluginGuest* BrowserPluginGuestManager::GetGuestByInstanceID(
    int instance_id,
    int embedder_render_process_id) const {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            instance_id)) {
    return NULL;
  }
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return NULL;
  return static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();
}

void BrowserPluginGuestManager::AddGuest(int instance_id,
                                         WebContentsImpl* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[instance_id] = guest_web_contents;
}

void BrowserPluginGuestManager::RemoveGuest(int instance_id) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) !=
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(instance_id);
}

bool BrowserPluginGuestManager::CanEmbedderAccessInstanceIDMaybeKill(
    int embedder_render_process_id,
    int instance_id) const {
  if (!CanEmbedderAccessInstanceID(embedder_render_process_id, instance_id)) {
    // The embedder process is trying to access a guest it does not own.
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        RenderProcessHost::FromID(embedder_render_process_id)->GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return false;
  }
  return true;
}

void BrowserPluginGuestManager::OnMessageReceived(const IPC::Message& message,
                                                  int render_process_id) {
  DCHECK(BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(message));
  int instance_id = 0;
  // All allowed messages must have instance_id as their first parameter.
  PickleIterator iter(message);
  bool success = iter.ReadInt(&instance_id);
  DCHECK(success);
  BrowserPluginGuest* guest =
      GetGuestByInstanceID(instance_id, render_process_id);
  if (!guest)
    return;
  guest->OnMessageReceivedFromEmbedder(message);
}

// static
bool BrowserPluginGuestManager::CanEmbedderAccessGuest(
    int embedder_render_process_id,
    BrowserPluginGuest* guest) {
  // The embedder can access the guest if it has not been attached and its
  // opener's embedder lives in the same process as the given embedder.
  if (!guest->attached()) {
    if (!guest->opener())
      return false;

    return embedder_render_process_id ==
        guest->opener()->embedder_web_contents()->GetRenderProcessHost()->
            GetID();
  }

  return embedder_render_process_id ==
      guest->embedder_web_contents()->GetRenderProcessHost()->GetID();
}

bool BrowserPluginGuestManager::CanEmbedderAccessInstanceID(
    int embedder_render_process_id,
    int instance_id) const {
  // The embedder is trying to access a guest with a negative or zero
  // instance ID.
  if (instance_id <= browser_plugin::kInstanceIDNone)
    return false;

  // The embedder is trying to access an instance ID that has not yet been
  // allocated by BrowserPluginGuestManager. This could cause instance ID
  // collisions in the future, and potentially give one embedder access to a
  // guest it does not own.
  if (instance_id > next_instance_id_)
    return false;

  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;
  BrowserPluginGuest* guest =
      static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();

  return CanEmbedderAccessGuest(embedder_render_process_id, guest);
}

SiteInstance* BrowserPluginGuestManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  for (GuestInstanceMap::const_iterator it =
       guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site)
      return it->second->GetSiteInstance();
  }
  return NULL;
}

bool BrowserPluginGuestManager::ForEachGuest(
    WebContentsImpl* embedder_web_contents, const GuestCallback& callback) {
  for (GuestInstanceMap::iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    BrowserPluginGuest* guest = it->second->GetBrowserPluginGuest();
    if (embedder_web_contents != guest->embedder_web_contents())
      continue;

    if (callback.Run(guest))
      return true;
  }
  return false;
}

}  // namespace content
