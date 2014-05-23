// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/guest_view_manager.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/guest_view/guest_view_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::SiteInstance;
using content::WebContents;

// A WebContents does not immediately have a RenderProcessHost. It acquires one
// on initial navigation. This observer exists until that initial navigation in
// order to grab the ID if tis RenderProcessHost so that it can register it as
// a guest.
class GuestWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit GuestWebContentsObserver(WebContents* guest_web_contents)
      : WebContentsObserver(guest_web_contents) {
  }

  virtual ~GuestWebContentsObserver() {
  }

  // WebContentsObserver:
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE {
    GuestViewManager::FromBrowserContext(web_contents()->GetBrowserContext())->
        AddRenderProcessHostID(web_contents()->GetRenderProcessHost()->GetID());
    delete this;
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestWebContentsObserver);
};

GuestViewManager::GuestViewManager(content::BrowserContext* context)
    : current_instance_id_(0), last_instance_id_removed_(0), context_(context) {
}

GuestViewManager::~GuestViewManager() {}

// static.
GuestViewManager* GuestViewManager::FromBrowserContext(
    BrowserContext* context) {
  GuestViewManager* guest_manager =
      static_cast<GuestViewManager*>(context->GetUserData(
          guestview::kGuestViewManagerKeyName));
  if (!guest_manager) {
    guest_manager = new GuestViewManager(context);
    context->SetUserData(guestview::kGuestViewManagerKeyName, guest_manager);
  }
  return guest_manager;
}

content::WebContents* GuestViewManager::GetGuestByInstanceIDSafely(
    int guest_instance_id,
    int embedder_render_process_id) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    return NULL;
  }
  return GetGuestByInstanceID(guest_instance_id, embedder_render_process_id);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

content::WebContents* GuestViewManager::CreateGuest(
    content::SiteInstance* embedder_site_instance,
    int instance_id,
    const std::string& storage_partition_id,
    bool persist_storage,
    scoped_ptr<base::DictionaryValue> extra_params) {
  content::RenderProcessHost* embedder_process_host =
      embedder_site_instance->GetProcess();
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!base::IsStringUTF8(storage_partition_id)) {
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
      storage_partition_id, false);
  // The SiteInstance of a given webview tag is based on the fact that it's
  // a guest process in addition to which platform application the tag
  // belongs to and what storage partition is in use, rather than the URL
  // that the tag is being navigated to.
  GURL guest_site(base::StringPrintf("%s://%s/%s?%s",
                                     content::kGuestScheme,
                                     host.c_str(),
                                     persist_storage ? "persist" : "",
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
  WebContents::CreateParams create_params(
      embedder_site_instance->GetBrowserContext(),
      guest_site_instance);
  create_params.guest_instance_id = instance_id;
  create_params.guest_extra_params.reset(extra_params.release());
  return WebContents::Create(create_params);
}

void GuestViewManager::MaybeGetGuestByInstanceIDOrKill(
    int guest_instance_id,
    int embedder_render_process_id,
    const GuestByInstanceIDCallback& callback) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    // If we kill the embedder, then don't bother calling back.
    return;
  }
  content::WebContents* guest_web_contents =
      GetGuestByInstanceID(guest_instance_id, embedder_render_process_id);
  callback.Run(guest_web_contents);
}

SiteInstance* GuestViewManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  for (GuestInstanceMap::const_iterator it =
       guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site)
      return it->second->GetSiteInstance();
  }
  return NULL;
}

bool GuestViewManager::ForEachGuest(WebContents* embedder_web_contents,
                                    const GuestCallback& callback) {
  for (GuestInstanceMap::iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContents* guest = it->second;
    GuestViewBase* guest_view = GuestViewBase::FromWebContents(guest);
    if (embedder_web_contents != guest_view->embedder_web_contents())
      continue;

    if (callback.Run(guest))
      return true;
  }
  return false;
}

void GuestViewManager::AddGuest(int guest_instance_id,
                                WebContents* guest_web_contents) {
  CHECK(!ContainsKey(guest_web_contents_by_instance_id_, guest_instance_id));
  CHECK(CanUseGuestInstanceID(guest_instance_id));
  guest_web_contents_by_instance_id_[guest_instance_id] = guest_web_contents;
  // This will add the RenderProcessHost ID when we get one.
  new GuestWebContentsObserver(guest_web_contents);
}

void GuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestInstanceMap::iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  render_process_host_id_multiset_.erase(
      it->second->GetRenderProcessHost()->GetID());
  guest_web_contents_by_instance_id_.erase(it);

  // All the instance IDs that lie within [0, last_instance_id_removed_]
  // are invalid.
  // The remaining sparse invalid IDs are kept in |removed_instance_ids_| set.
  // The following code compacts the set by incrementing
  // |last_instance_id_removed_|.
  if (guest_instance_id == last_instance_id_removed_ + 1) {
    ++last_instance_id_removed_;
    // Compact.
    std::set<int>::iterator iter = removed_instance_ids_.begin();
    while (iter != removed_instance_ids_.end()) {
      int instance_id = *iter;
      // The sparse invalid IDs must not lie within
      // [0, last_instance_id_removed_]
      DCHECK(instance_id > last_instance_id_removed_);
      if (instance_id != last_instance_id_removed_ + 1)
        break;
      ++last_instance_id_removed_;
      removed_instance_ids_.erase(iter++);
    }
  } else {
    removed_instance_ids_.insert(guest_instance_id);
  }
}

void GuestViewManager::AddRenderProcessHostID(int render_process_host_id) {
  render_process_host_id_multiset_.insert(render_process_host_id);
}

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int guest_instance_id,
    int embedder_render_process_id) {
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return NULL;
  return it->second;
}

bool GuestViewManager::CanEmbedderAccessInstanceIDMaybeKill(
    int embedder_render_process_id,
    int guest_instance_id) {
  if (!CanEmbedderAccessInstanceID(embedder_render_process_id,
                                  guest_instance_id)) {
    // The embedder process is trying to access a guest it does not own.
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        content::RenderProcessHost::FromID(embedder_render_process_id)->
            GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return false;
  }
  return true;
}

bool GuestViewManager::CanUseGuestInstanceID(int guest_instance_id) {
  if (guest_instance_id <= last_instance_id_removed_)
    return false;
  return !ContainsKey(removed_instance_ids_, guest_instance_id);
}

bool GuestViewManager::CanEmbedderAccessInstanceID(
    int embedder_render_process_id,
    int guest_instance_id) {
  // The embedder is trying to access a guest with a negative or zero
  // instance ID.
  if (guest_instance_id <= guestview::kInstanceIDNone)
    return false;

  // The embedder is trying to access an instance ID that has not yet been
  // allocated by GuestViewManager. This could cause instance ID
  // collisions in the future, and potentially give one embedder access to a
  // guest it does not own.
  if (guest_instance_id > current_instance_id_)
    return false;

  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;

  GuestViewBase* guest_view = GuestViewBase::FromWebContents(it->second);
  if (!guest_view)
    return false;

  return CanEmbedderAccessGuest(embedder_render_process_id, guest_view);
}

bool GuestViewManager::CanEmbedderAccessGuest(int embedder_render_process_id,
                                              GuestViewBase* guest) {
  // The embedder can access the guest if it has not been attached and its
  // opener's embedder lives in the same process as the given embedder.
  if (!guest->attached()) {
    if (!guest->GetOpener())
      return false;

    return embedder_render_process_id ==
        guest->GetOpener()->embedder_web_contents()->GetRenderProcessHost()->
            GetID();
  }

  return embedder_render_process_id ==
      guest->embedder_web_contents()->GetRenderProcessHost()->GetID();
}
