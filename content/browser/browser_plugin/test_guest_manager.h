// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_H_

#include <map>

#include "base/memory/singleton.h"
#include "content/public/browser/browser_plugin_guest_manager.h"

namespace content {

class MessageLoopRunner;
class WebContentsImpl;

// This class is temporary until BrowserPluginHostTest.* tests are entirely
// moved out of content.
class TestGuestManager : public BrowserPluginGuestManager {
 public:
  virtual ~TestGuestManager();

  static TestGuestManager* GetInstance();

  void AddGuest(int guest_instance_id, WebContents* guest_web_contents);
  void RemoveGuest(int guest_instance_id);
  SiteInstance* GetGuestSiteInstance(const GURL& guest_site);

  // Waits until at least one guest is added to this guest manager.
  WebContentsImpl* WaitForGuestAdded();

  // BrowserPluginGuestManager implementation.
  virtual WebContents* CreateGuest(
      SiteInstance* embedder_site_instance,
      int instance_id,
      const std::string& storage_partition_id,
      bool persist_storage,
      scoped_ptr<base::DictionaryValue> extra_params) OVERRIDE;
  virtual int GetNextInstanceID() OVERRIDE;
  virtual void MaybeGetGuestByInstanceIDOrKill(
      int guest_instance_id,
      int embedder_render_process_id,
      const GuestByInstanceIDCallback& callback) OVERRIDE;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<TestGuestManager>;
  TestGuestManager();

  // Contains guests' WebContents, mapping from their instance ids.
  typedef std::map<int, WebContents*> GuestInstanceMap;
  GuestInstanceMap guest_web_contents_by_instance_id_;
  WebContentsImpl* last_guest_added_;
  int next_instance_id_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_H_
