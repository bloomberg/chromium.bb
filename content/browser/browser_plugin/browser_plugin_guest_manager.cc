// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"

#include "base/command_line.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_export.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
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
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  SiteInstance* guest_site_instance = NULL;
  int embedder_render_process_id =
      embedder_site_instance->GetProcess()->GetID();
  BrowserPluginGuest* guest =
      GetGuestByInstanceID(instance_id, embedder_render_process_id);
  CHECK(!guest);

  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!IsStringUTF8(params.storage_partition_id)) {
    content::RecordAction(UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        embedder_site_instance->GetProcess()->GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return NULL;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSitePerProcess)) {
    // When --site-per-process is specified, the behavior of BrowserPlugin
    // as <webview> is broken and we use it for rendering out-of-process
    // iframes instead. We use the src URL sent by the renderer to find the
    // right process in which to place this instance.
    // Note: Since BrowserPlugin doesn't support cross-process navigation,
    // the instance will stay in the initially assigned process, regardless
    // of the site it is navigated to.
    // TODO(nasko): Fix this, and such that cross-process navigations are
    // supported.
    guest_site_instance =
        embedder_site_instance->GetRelatedSiteInstance(GURL(params.src));
  } else {
    const std::string& host = embedder_site_instance->GetSiteURL().host();

    std::string url_encoded_partition = net::EscapeQueryParamValue(
        params.storage_partition_id, false);
    // The SiteInstance of a given webview tag is based on the fact that it's
    // a guest process in addition to which platform application the tag
    // belongs to and what storage partition is in use, rather than the URL
    // that the tag is being navigated to.
    GURL guest_site(
        base::StringPrintf("%s://%s/%s?%s", chrome::kGuestScheme,
                            host.c_str(),
                            params.persist_storage ? "persist" : "",
                            url_encoded_partition.c_str()));

    // If we already have a webview tag in the same app using the same storage
    // partition, we should use the same SiteInstance so the existing tag and
    // the new tag can script each other.
    guest_site_instance = GetGuestSiteInstance(guest_site);
    if (!guest_site_instance) {
      // Create the SiteInstance in a new BrowsingInstance, which will ensure
      // that webview tags are also not allowed to send messages across
      // different partitions.
      guest_site_instance = SiteInstance::CreateForURL(
          embedder_site_instance->GetBrowserContext(), guest_site);
    }
  }

  return WebContentsImpl::CreateGuest(
      embedder_site_instance->GetBrowserContext(),
      guest_site_instance,
      instance_id);
}

BrowserPluginGuest* BrowserPluginGuestManager::GetGuestByInstanceID(
    int instance_id,
    int embedder_render_process_id) const {
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return NULL;

  BrowserPluginGuest* guest =
      static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();
  if (!CanEmbedderAccessGuest(embedder_render_process_id, guest)) {
    // The embedder process is trying to access a guest it does not own.
    content::RecordAction(UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        RenderProcessHost::FromID(embedder_render_process_id)->GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return NULL;
  }
  return guest;
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

void BrowserPluginGuestManager::OnMessageReceived(const IPC::Message& message,
                                                  int render_process_id) {
  if (BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(message)) {
    int instance_id = 0;
    // All allowed messages must have instance_id as their first parameter.
    PickleIterator iter(message);
    bool success = iter.ReadInt(&instance_id);
    DCHECK(success);
    BrowserPluginGuest* guest =
        GetGuestByInstanceID(instance_id, render_process_id);
    if (guest && guest->OnMessageReceivedFromEmbedder(message))
      return;
  }
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuestManager, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_BuffersSwappedACK,
                        OnUnhandledSwapBuffersACK)
  IPC_END_MESSAGE_MAP()
}

//static
bool BrowserPluginGuestManager::CanEmbedderAccessGuest(
    int embedder_render_process_id,
    BrowserPluginGuest* guest) {
  // An embedder can access |guest| if the guest has not been attached and its
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

// We only get here during teardown if we have one last buffer pending,
// otherwise the ACK is handled by the guest.
void BrowserPluginGuestManager::OnUnhandledSwapBuffersACK(
    int instance_id,
    int route_id,
    int gpu_host_id,
    const std::string& mailbox_name,
    uint32 sync_point) {
  BrowserPluginGuest::AcknowledgeBufferPresent(route_id,
                                               gpu_host_id,
                                               mailbox_name,
                                               sync_point);
}

}  // namespace content
