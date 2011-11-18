// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/browser_online_state_observer.h"

#include "content/common/view_messages.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "net/base/network_change_notifier.h"

BrowserOnlineStateObserver::BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::AddOnlineStateObserver(this);
}

BrowserOnlineStateObserver::~BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
}

void BrowserOnlineStateObserver::OnOnlineStateChanged(bool online) {
  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_NetworkStateChanged(online));
  }
}
