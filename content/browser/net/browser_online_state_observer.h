// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
#define CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "net/base/network_change_notifier.h"

// Listens for changes to the online state and manages sending
// updates to each RenderProcess via RenderProcessHost IPC.
class CONTENT_EXPORT BrowserOnlineStateObserver
    : public net::NetworkChangeNotifier::OnlineStateObserver {
 public:
  BrowserOnlineStateObserver();
  virtual ~BrowserOnlineStateObserver();

  // OnlineStateObserver implementation.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserOnlineStateObserver);
};

#endif  // CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
