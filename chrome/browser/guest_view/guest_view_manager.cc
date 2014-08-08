// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/guest_view_manager.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/guest_view/guest_view_constants.h"
#include "chrome/browser/guest_view/guest_view_manager_factory.h"
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

// static
GuestViewManagerFactory* GuestViewManager::factory_ = NULL;

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
    if (factory_) {
      guest_manager = factory_->CreateGuestViewManager(context);
    } else {
      guest_manager = new GuestViewManager(context);
    }
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
  return GetGuestByInstanceID(guest_instance_id);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

void GuestViewManager::CreateGuest(const std::string& view_type,
                                   const std::string& embedder_extension_id,
                                   content::WebContents* embedder_web_contents,
                                   const base::DictionaryValue& create_params,
                                   const WebContentsCreatedCallback& callback) {
  int guest_instance_id = GetNextInstanceID();
  GuestViewBase* guest =
      GuestViewBase::Create(context_, guest_instance_id, view_type);
  if (!guest) {
    callback.Run(NULL);
    return;
  }
  guest->Init(
      embedder_extension_id, embedder_web_contents, create_params, callback);
}

content::WebContents* GuestViewManager::CreateGuestWithWebContentsParams(
    const std::string& view_type,
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const content::WebContents::CreateParams& create_params) {
  int guest_instance_id = GetNextInstanceID();
  GuestViewBase* guest =
      GuestViewBase::Create(context_, guest_instance_id, view_type);
  if (!guest)
    return NULL;
  content::WebContents::CreateParams guest_create_params(create_params);
  guest_create_params.guest_delegate = guest;
  content::WebContents* guest_web_contents =
      WebContents::Create(guest_create_params);
  guest->InitWithWebContents(embedder_extension_id,
                             embedder_render_process_id,
                             guest_web_contents);
  return guest_web_contents;
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
      GetGuestByInstanceID(guest_instance_id);
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
}

void GuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestInstanceMap::iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
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

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int guest_instance_id) {
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

  // We might get some late arriving messages at tear down. Let's let the
  // embedder tear down in peace.
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;

  GuestViewBase* guest_view = GuestViewBase::FromWebContents(it->second);
  if (!guest_view)
    return false;

  return embedder_render_process_id == guest_view->embedder_render_process_id();
}
