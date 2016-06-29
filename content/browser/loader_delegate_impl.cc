// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader_delegate_impl.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

LoaderDelegateImpl::~LoaderDelegateImpl() {}

void LoaderDelegateImpl::LoadStateChanged(
    int child_id,
    int route_id,
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LoaderDelegateImpl::NotifyLoadStateChangedOnUI,
                 base::Unretained(this), child_id, route_id, url, load_state,
                 upload_position, upload_size));
}

void LoaderDelegateImpl::NotifyLoadStateChangedOnUI(
    int child_id,
    int route_id,
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  RenderViewHostImpl* view = RenderViewHostImpl::FromID(child_id, route_id);
  if (view)
    view->LoadStateChanged(url, load_state, upload_position, upload_size);
}

}  // namespace content
