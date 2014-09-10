// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(GetContentClient()->browser()->GetDevToolsManagerDelegate()),
      client_count_(0) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!client_count_);
}

void DevToolsManager::OnClientAttached() {
  if (!client_count_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Attach));
  }
  client_count_++;
}

void DevToolsManager::OnClientDetached() {
  client_count_--;
  if (!client_count_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Detach));
  }
}

}  // namespace content
