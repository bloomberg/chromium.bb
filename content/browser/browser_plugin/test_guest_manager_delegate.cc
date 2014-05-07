// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_guest_manager_delegate.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace content {

TestGuestManagerDelegate::TestGuestManagerDelegate()
    : next_instance_id_(0) {
}

TestGuestManagerDelegate::~TestGuestManagerDelegate() {
}

// static.
TestGuestManagerDelegate* TestGuestManagerDelegate::GetInstance() {
  return Singleton<TestGuestManagerDelegate>::get();
}

int TestGuestManagerDelegate::GetNextInstanceID() {
  return ++next_instance_id_;
}

void TestGuestManagerDelegate::AddGuest(
    int guest_instance_id,
    WebContents* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(guest_instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[guest_instance_id] = guest_web_contents;
}

void TestGuestManagerDelegate::RemoveGuest(
    int guest_instance_id) {
  GuestInstanceMap::iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(it);
}

void TestGuestManagerDelegate::MaybeGetGuestByInstanceIDOrKill(
    int guest_instance_id,
    int embedder_render_process_id,
    const GuestByInstanceIDCallback& callback) {
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end()) {
    callback.Run(NULL);
    return;
  }
  callback.Run(it->second);
}

SiteInstance* TestGuestManagerDelegate::GetGuestSiteInstance(
  const GURL& guest_site) {
  for (GuestInstanceMap::const_iterator it =
       guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site)
      return it->second->GetSiteInstance();
  }
  return NULL;
}

bool TestGuestManagerDelegate::ForEachGuest(
    WebContents* embedder_web_contents,
    const GuestCallback& callback) {
  for (GuestInstanceMap::iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContents* guest = it->second;
    if (embedder_web_contents != guest->GetEmbedderWebContents())
      continue;

    if (callback.Run(guest))
      return true;
  }
  return false;
}

}  // namespace content
