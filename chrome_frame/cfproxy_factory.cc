// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/cfproxy_private.h"
#include "base/process_util.h"

IPC::Message::Sender* CFProxyTraits::CreateChannel(const std::string& id,
    IPC::Channel::Listener* listener) {
  IPC::Channel* c = new IPC::Channel(id, IPC::Channel::MODE_SERVER, listener);
  if (c)
    c->Connect();  // must be called on the IPC thread.
  return c;
}

void CFProxyTraits::CloseChannel(IPC::Message::Sender* s) {
  IPC::Channel *c  = static_cast<IPC::Channel*>(s);
  delete c;
}

bool CFProxyTraits::LaunchApp(const std::wstring& cmd_line) {
  bool ok = base::LaunchApp(cmd_line, false, false, NULL);
  return ok;
}

//////////////////////////////////////////////////////////
// ChromeProxyFactory

ChromeProxyFactory::ChromeProxyFactory() {
}

ChromeProxyFactory::~ChromeProxyFactory() {
  base::AutoLock lock(lock_);
  ProxyMap::iterator it = proxies_.begin();
  for (; it != proxies_.end(); ++it) {
    ChromeProxy* proxy = it->second;
    delete proxy;
  }
  proxies_.clear();
}

void ChromeProxyFactory::GetProxy(ChromeProxyDelegate* delegate,
                                  const ProxyParams& params) {
  base::AutoLock lock(lock_);
  ChromeProxy* proxy = NULL;
  // TODO(stoyan): consider extra_params/timeout
  ProxyMap::iterator it = proxies_.find(params.profile);
  if (it == proxies_.end()) {
    proxy = CreateProxy();
    proxy->Init(params);
    proxies_.insert(make_pair(params.profile, proxy));
  } else {
    proxy = it->second;
  }

  proxy->AddDelegate(delegate);
  // TODO(stoyan): ::DeleteTimerQueueTimer (if any).
}

bool ChromeProxyFactory::ReleaseProxy(ChromeProxyDelegate* delegate,
                                      const std::string& profile) {
  base::AutoLock lock(lock_);
  ProxyMap::iterator it = proxies_.find(profile);
  if (it == proxies_.end())
    return false;

  if (0 == it->second->RemoveDelegate(delegate)) {
    // This was the last delegate for this proxy.
    // TODO(stoyan): Use ::CreateTimerQueueTimer to schedule destroy of
    // the proxy in a reasonable timeout.
  }

  return true;
}

static CFProxyTraits g_default_traits;
ChromeProxy* ChromeProxyFactory::CreateProxy() {
  ChromeProxy* p = new CFProxy(&g_default_traits);
  return p;
}
