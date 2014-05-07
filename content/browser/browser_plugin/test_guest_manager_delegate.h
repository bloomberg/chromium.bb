// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_DELEGATE_H_

#include <map>

#include "base/memory/singleton.h"
#include "content/public/browser/browser_plugin_guest_manager_delegate.h"

namespace content {

// This class is temporary until BrowserPluginHostTest.* tests are entirely
// moved out of content.
class TestGuestManagerDelegate : public BrowserPluginGuestManagerDelegate {
 public:
  virtual ~TestGuestManagerDelegate();

  static TestGuestManagerDelegate* GetInstance();

  // BrowserPluginGuestManagerDelegate implementation.
  virtual int GetNextInstanceID() OVERRIDE;
  virtual void AddGuest(int guest_instance_id,
                        WebContents* guest_web_contents) OVERRIDE;
  virtual void RemoveGuest(int guest_instance_id) OVERRIDE;
  virtual void MaybeGetGuestByInstanceIDOrKill(
      int guest_instance_id,
      int embedder_render_process_id,
      const GuestByInstanceIDCallback& callback) OVERRIDE;
  virtual SiteInstance* GetGuestSiteInstance(
      const GURL& guest_site) OVERRIDE;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<TestGuestManagerDelegate>;
  TestGuestManagerDelegate();
  // Contains guests' WebContents, mapping from their instance ids.
  typedef std::map<int, WebContents*> GuestInstanceMap;
  GuestInstanceMap guest_web_contents_by_instance_id_;

  int next_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestManagerDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_GUEST_MANAGER_DELEGATE_H_
