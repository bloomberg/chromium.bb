// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_render_thread_observer.h"

#include "build/build_config.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"
#include "content/public/renderer/render_thread.h"

namespace chromecast {
namespace shell {

CastRenderThreadObserver::CastRenderThreadObserver() {
  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(this);
  CreateCustomFilters();
}

CastRenderThreadObserver::~CastRenderThreadObserver() {
  // CastRenderThreadObserver outlives content::RenderThread.
  // No need to explicitly call RemoveObserver in teardown.
}

void CastRenderThreadObserver::CreateCustomFilters() {
#if !defined(OS_ANDROID)
  content::RenderThread* thread = content::RenderThread::Get();
  cma_message_filter_proxy_ =
      new media::CmaMessageFilterProxy(thread->GetIOTaskRunner());
  thread->AddFilter(cma_message_filter_proxy_.get());
#endif  // !defined(OS_ANDROID)
}

void CastRenderThreadObserver::OnRenderProcessShutdown() {
#if !defined(OS_ANDROID)
  content::RenderThread* thread = content::RenderThread::Get();
  if (cma_message_filter_proxy_.get()) {
    thread->RemoveFilter(cma_message_filter_proxy_.get());
    cma_message_filter_proxy_ = nullptr;
  }
#endif  // !defined(OS_ANDROID)
}

}  // namespace shell
}  // namespace chromecast
