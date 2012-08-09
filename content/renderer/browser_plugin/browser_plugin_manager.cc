// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/browser_plugin/browser_plugin.h"

namespace content {

static base::LazyInstance<base::ThreadLocalPointer<
    BrowserPluginManager> > lazy_tls = LAZY_INSTANCE_INITIALIZER;

BrowserPluginManager* BrowserPluginManager::Get() {
  return lazy_tls.Pointer()->Get();
}

BrowserPluginManager::BrowserPluginManager() {
  lazy_tls.Pointer()->Set(this);
  RenderThread::Get()->AddObserver(this);
}

BrowserPluginManager::~BrowserPluginManager() {
  lazy_tls.Pointer()->Set(NULL);
}

void BrowserPluginManager::AddBrowserPlugin(
    int instance_id,
    BrowserPlugin* browser_plugin) {
  DCHECK(CalledOnValidThread());
  instances_.AddWithID(browser_plugin, instance_id);
}

void BrowserPluginManager::RemoveBrowserPlugin(int instance_id) {
  DCHECK(CalledOnValidThread());
  instances_.Remove(instance_id);
}

BrowserPlugin* BrowserPluginManager::GetBrowserPlugin(int instance_id) const {
  DCHECK(CalledOnValidThread());
  return instances_.Lookup(instance_id);
}

}  // namespace content
