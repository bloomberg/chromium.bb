// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_manager.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

// static
GuestViewManagerFactory* GuestViewManager::factory_ = nullptr;

GuestViewManager::GuestViewManager(content::BrowserContext* context)
    : current_instance_id_(0), last_instance_id_removed_(0), context_(context) {
}

GuestViewManager::~GuestViewManager() {}

// static
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

// static
GuestViewManager* GuestViewManager::FromBrowserContextIfAvailable(
    BrowserContext* context) {
  return static_cast<GuestViewManager*>(context->GetUserData(
      guestview::kGuestViewManagerKeyName));
}

content::WebContents* GuestViewManager::GetGuestByInstanceIDSafely(
    int guest_instance_id,
    int embedder_render_process_id) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    return nullptr;
  }
  return GetGuestByInstanceID(guest_instance_id);
}

void GuestViewManager::AttachGuest(int embedder_process_id,
                                   int element_instance_id,
                                   int guest_instance_id,
                                   const base::DictionaryValue& attach_params) {
  auto guest_view = GuestViewBase::From(embedder_process_id, guest_instance_id);
  if (!guest_view)
    return;

  ElementInstanceKey key(embedder_process_id, element_instance_id);
  auto it = instance_id_map_.find(key);
  // If there is an existing guest attached to the element, then destroy the
  // existing guest.
  if (it != instance_id_map_.end()) {
    int old_guest_instance_id = it->second;
    if (old_guest_instance_id == guest_instance_id)
      return;

    auto old_guest_view = GuestViewBase::From(embedder_process_id,
                                              old_guest_instance_id);
    old_guest_view->Destroy();
  }
  instance_id_map_[key] = guest_instance_id;
  reverse_instance_id_map_[guest_instance_id] = key;
  guest_view->SetAttachParams(attach_params);
}

void GuestViewManager::DetachGuest(GuestViewBase* guest) {
  if (!guest->attached())
    return;

  auto reverse_it = reverse_instance_id_map_.find(guest->guest_instance_id());
  if (reverse_it == reverse_instance_id_map_.end())
    return;

  const ElementInstanceKey& key = reverse_it->second;

  auto it = instance_id_map_.find(key);
  DCHECK(it != instance_id_map_.end());

  reverse_instance_id_map_.erase(reverse_it);
  instance_id_map_.erase(it);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

void GuestViewManager::CreateGuest(const std::string& view_type,
                                   content::WebContents* owner_web_contents,
                                   const base::DictionaryValue& create_params,
                                   const WebContentsCreatedCallback& callback) {
  auto guest = GuestViewBase::Create(owner_web_contents, view_type);
  if (!guest) {
    callback.Run(nullptr);
    return;
  }
  guest->Init(create_params, callback);
}

content::WebContents* GuestViewManager::CreateGuestWithWebContentsParams(
    const std::string& view_type,
    content::WebContents* owner_web_contents,
    const content::WebContents::CreateParams& create_params) {
  auto guest = GuestViewBase::Create(owner_web_contents, view_type);
  if (!guest)
    return nullptr;
  content::WebContents::CreateParams guest_create_params(create_params);
  guest_create_params.guest_delegate = guest;
  auto guest_web_contents = WebContents::Create(guest_create_params);
  guest->InitWithWebContents(base::DictionaryValue(), guest_web_contents);
  return guest_web_contents;
}

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int owner_process_id,
    int element_instance_id) {
  int guest_instance_id = GetGuestInstanceIDForElementID(owner_process_id,
                                                         element_instance_id);
  if (guest_instance_id == guestview::kInstanceIDNone)
    return nullptr;

  return GetGuestByInstanceID(guest_instance_id);
}

int GuestViewManager::GetGuestInstanceIDForElementID(int owner_process_id,
                                                     int element_instance_id) {
  auto iter = instance_id_map_.find(
      ElementInstanceKey(owner_process_id, element_instance_id));
  if (iter == instance_id_map_.end())
    return guestview::kInstanceIDNone;
  return iter->second;
}

SiteInstance* GuestViewManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  for (const auto& guest : guest_web_contents_by_instance_id_) {
    if (guest.second->GetSiteInstance()->GetSiteURL() == guest_site)
      return guest.second->GetSiteInstance();
  }
  return nullptr;
}

bool GuestViewManager::ForEachGuest(WebContents* owner_web_contents,
                                    const GuestCallback& callback) {
  for (const auto& guest : guest_web_contents_by_instance_id_) {
    auto guest_view = GuestViewBase::FromWebContents(guest.second);
    if (guest_view->owner_web_contents() != owner_web_contents)
      continue;

    if (callback.Run(guest_view->web_contents()))
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
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(it);

  auto id_iter = reverse_instance_id_map_.find(guest_instance_id);
  if (id_iter != reverse_instance_id_map_.end()) {
    const ElementInstanceKey& instance_id_key = id_iter->second;
    instance_id_map_.erase(instance_id_map_.find(instance_id_key));
    reverse_instance_id_map_.erase(id_iter);
  }

  // All the instance IDs that lie within [0, last_instance_id_removed_]
  // are invalid.
  // The remaining sparse invalid IDs are kept in |removed_instance_ids_| set.
  // The following code compacts the set by incrementing
  // |last_instance_id_removed_|.
  if (guest_instance_id == last_instance_id_removed_ + 1) {
    ++last_instance_id_removed_;
    // Compact.
    auto iter = removed_instance_ids_.begin();
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
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return nullptr;
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
    content::RenderProcessHost::FromID(embedder_render_process_id)
        ->Shutdown(content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
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
  auto it = guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;

  auto guest_view = GuestViewBase::FromWebContents(it->second);
  if (!guest_view)
    return false;

  return embedder_render_process_id ==
      guest_view->owner_web_contents()->GetRenderProcessHost()->GetID();
}

GuestViewManager::ElementInstanceKey::ElementInstanceKey()
    : embedder_process_id(content::ChildProcessHost::kInvalidUniqueID),
      element_instance_id(content::ChildProcessHost::kInvalidUniqueID) {
}

GuestViewManager::ElementInstanceKey::ElementInstanceKey(
    int embedder_process_id,
    int element_instance_id)
    : embedder_process_id(embedder_process_id),
      element_instance_id(element_instance_id) {
}

bool GuestViewManager::ElementInstanceKey::operator<(
    const GuestViewManager::ElementInstanceKey& other) const {
  if (embedder_process_id != other.embedder_process_id)
    return embedder_process_id < other.embedder_process_id;

  return element_instance_id < other.element_instance_id;
}

bool GuestViewManager::ElementInstanceKey::operator==(
    const GuestViewManager::ElementInstanceKey& other) const {
  return (embedder_process_id == other.embedder_process_id) &&
    (element_instance_id == other.element_instance_id);
}

}  // namespace extensions
