// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_guest_manager.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "net/base/escape.h"

namespace content {

class GuestWebContentsObserver
    : public WebContentsObserver {
 public:
  explicit GuestWebContentsObserver(WebContents* guest_web_contents)
      : WebContentsObserver(guest_web_contents) {
    BrowserPluginGuest* guest =
        static_cast<WebContentsImpl*>(guest_web_contents)->
            GetBrowserPluginGuest();
    guest_instance_id_ = guest->instance_id();
  }

  virtual ~GuestWebContentsObserver() {
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    TestGuestManager::GetInstance()->RemoveGuest(guest_instance_id_);
    delete this;
  }

 private:
  int guest_instance_id_;
  DISALLOW_COPY_AND_ASSIGN(GuestWebContentsObserver);
};

TestGuestManager::TestGuestManager()
    : last_guest_added_(NULL),
      next_instance_id_(0) {
}

TestGuestManager::~TestGuestManager() {
}

// static.
TestGuestManager* TestGuestManager::GetInstance() {
  return Singleton<TestGuestManager>::get();
}

WebContentsImpl* TestGuestManager::WaitForGuestAdded() {
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

WebContents* TestGuestManager::CreateGuest(
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
                                     kGuestScheme,
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
  WebContentsImpl* guest_web_contents = static_cast<WebContentsImpl*>(
      WebContents::Create(create_params));
  BrowserPluginGuest* guest = guest_web_contents->GetBrowserPluginGuest();
  guest_web_contents->GetBrowserPluginGuest()->SetDelegate(
      new TestBrowserPluginGuestDelegate(guest));
  AddGuest(instance_id, guest_web_contents);
  return guest_web_contents;
}

int TestGuestManager::GetNextInstanceID() {
  return ++next_instance_id_;
}

void TestGuestManager::AddGuest(
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

void TestGuestManager::RemoveGuest(
    int guest_instance_id) {
  GuestInstanceMap::iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(it);
}

void TestGuestManager::MaybeGetGuestByInstanceIDOrKill(
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

SiteInstance* TestGuestManager::GetGuestSiteInstance(
  const GURL& guest_site) {
  for (GuestInstanceMap::const_iterator it =
       guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site)
      return it->second->GetSiteInstance();
  }
  return NULL;
}

bool TestGuestManager::ForEachGuest(
    WebContents* embedder_web_contents,
    const GuestCallback& callback) {
  for (GuestInstanceMap::iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContentsImpl* guest_web_contents =
        static_cast<WebContentsImpl*>(it->second);
    BrowserPluginGuest* guest = guest_web_contents->GetBrowserPluginGuest();
    if (embedder_web_contents != guest->embedder_web_contents())
      continue;

    if (callback.Run(guest_web_contents))
      return true;
  }
  return false;
}

}  // namespace content
