// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_controller_handle.h"

#include "base/bind.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_controller.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

DevToolsNetworkControllerHandle::DevToolsNetworkControllerHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DevToolsNetworkControllerHandle::~DevToolsNetworkControllerHandle() {}

void DevToolsNetworkControllerHandle::SetNetworkState(
    const std::string& client_id,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DevToolsNetworkControllerHandle::SetNetworkStateOnIO,
                 base::Unretained(this), client_id, base::Passed(&conditions)));
}

DevToolsNetworkController* DevToolsNetworkControllerHandle::GetController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LazyInitialize();
  return controller_.get();
}

void DevToolsNetworkControllerHandle::LazyInitialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!controller_)
    controller_.reset(new DevToolsNetworkController);
}

void DevToolsNetworkControllerHandle::SetNetworkStateOnIO(
    const std::string& client_id,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LazyInitialize();
  controller_->SetNetworkState(client_id, conditions.Pass());
}
