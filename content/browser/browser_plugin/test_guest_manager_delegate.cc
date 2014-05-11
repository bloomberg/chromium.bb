// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_guest_manager_delegate.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "net/base/escape.h"

namespace content {

class GuestWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit GuestWebContentsObserver(WebContents* guest_web_contents)
      : WebContentsObserver(guest_web_contents),
        guest_instance_id_(guest_web_contents->GetEmbeddedInstanceID()) {
  }

  virtual ~GuestWebContentsObserver() {
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    TestGuestManagerDelegate::GetInstance()->RemoveGuest(guest_instance_id_);
    delete this;
  }

 private:
  int guest_instance_id_;
  DISALLOW_COPY_AND_ASSIGN(GuestWebContentsObserver);
};

TestGuestManagerDelegate::TestGuestManagerDelegate()
    : last_guest_added_(NULL),
      next_instance_id_(0) {
}

TestGuestManagerDelegate::~TestGuestManagerDelegate() {
}

// static.
TestGuestManagerDelegate* TestGuestManagerDelegate::GetInstance() {
  return Singleton<TestGuestManagerDelegate>::get();
}

WebContentsImpl* TestGuestManagerDelegate::WaitForGuestAdded() {
  // Check if guests were already created.
  if (last_guest_added_) {
    WebContentsImpl* last_guest_added = last_guest_added_;
    last_guest_added_ = NULL;
    return last_guest_added;
  }
  // Wait otherwise.
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
  WebContentsImpl* last_guest_added = last_guest_added_;
  last_guest_added_ = NULL;
  return last_guest_added;
}

content::WebContents* TestGuestManagerDelegate::CreateGuest(
    SiteInstance* embedder_site_instance,
    int instance_id,
    const std::string& storage_partition_id,
    bool persist_storage,
    scoped_ptr<base::DictionaryValue> extra_params) {
  const GURL& embedder_site_url = embedder_site_instance->GetSiteURL();
  const std::string& host = embedder_site_url.host();

  std::string url_encoded_partition = net::EscapeQueryParamValue(
      storage_partition_id, false);
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
  WebContents* guest_web_contents = WebContents::Create(create_params);
  AddGuest(instance_id, guest_web_contents);
  return guest_web_contents;
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
  new GuestWebContentsObserver(guest_web_contents);
  last_guest_added_ = static_cast<WebContentsImpl*>(guest_web_contents);
  if (message_loop_runner_)
    message_loop_runner_->Quit();
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
