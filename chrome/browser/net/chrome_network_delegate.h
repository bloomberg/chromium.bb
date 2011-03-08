// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/network_delegate.h"

class ExtensionEventRouterForwarder;

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::NetworkDelegate {
 public:
  // If |profile_id| is the invalid profile, events will be broadcasted to all
  // profiles, otherwise, they will only be sent to the specified profile.
  explicit ChromeNetworkDelegate(
      ExtensionEventRouterForwarder* event_router,
      ProfileId profile_id);
  virtual ~ChromeNetworkDelegate();

 private:
  // NetworkDelegate methods:
  virtual bool OnBeforeURLRequest(net::URLRequest* request,
                                  net::CompletionCallback* callback);
  virtual void OnSendHttpRequest(net::HttpRequestHeaders* headers);
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  scoped_refptr<ExtensionEventRouterForwarder> event_router_;
  const ProfileId profile_id_;
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
