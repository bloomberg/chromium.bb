// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "content/public/browser/browser_plugin_guest_delegate.h"

#include "base/callback.h"
#include "content/public/common/page_transition_types.h"

namespace content {

class BrowserPluginGuest;
struct Referrer;

class TestBrowserPluginGuestDelegate : public BrowserPluginGuestDelegate {
 public:
  explicit TestBrowserPluginGuestDelegate(BrowserPluginGuest* guest);
  virtual ~TestBrowserPluginGuestDelegate();

  void ResetStates();

 private:
  void LoadURLWithParams(const GURL& url,
                         const Referrer& referrer,
                         PageTransition transition_type,
                         WebContents* web_contents);

  // Overridden from BrowserPluginGuestDelegate:
  virtual void Destroy() OVERRIDE;
  virtual void NavigateGuest(const std::string& src) OVERRIDE;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) OVERRIDE;

  BrowserPluginGuest* guest_;

  DestructionCallback destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginGuestDelegate);
};

}  // namespace content
#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_DELEGATE_H_
