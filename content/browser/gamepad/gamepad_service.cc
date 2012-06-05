// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_service.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "content/browser/gamepad/data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

namespace content {

GamepadService::GamepadService() :
    num_readers_(0) {
}

GamepadService::~GamepadService() {
}

GamepadService* GamepadService::GetInstance() {
  return Singleton<GamepadService,
                   LeakySingletonTraits<GamepadService> >::get();
}

void GamepadService::Start(
    GamepadDataFetcher* data_fetcher,
    RenderProcessHost* associated_rph) {
  num_readers_++;
  DCHECK(num_readers_ > 0);
  if (provider_ == NULL) {
    provider_.reset(new GamepadProvider);
    provider_->SetDataFetcher(data_fetcher);
  }
  provider_->Resume();

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GamepadService::RegisterForCloseNotification,
                 base::Unretained(this),
                 associated_rph));
}

void GamepadService::Terminate() {
  provider_.reset();
}

void GamepadService::RegisterForCloseNotification(RenderProcessHost* rph) {
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 Source<RenderProcessHost>(rph));
}

base::SharedMemoryHandle GamepadService::GetSharedMemoryHandle(
    base::ProcessHandle handle) {
  return provider_->GetRendererSharedMemoryHandle(handle);
}

void GamepadService::Stop(const NotificationSource& source) {
  --num_readers_;
  DCHECK(num_readers_ >= 0);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GamepadService::UnregisterForCloseNotification,
                 base::Unretained(this),
                 source));

  if (num_readers_ == 0)
    provider_->Pause();
}

void GamepadService::UnregisterForCloseNotification(
    const NotificationSource& source) {
  registrar_.Remove(this, NOTIFICATION_RENDERER_PROCESS_CLOSED, source);
}

void GamepadService::Observe(int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NOTIFICATION_RENDERER_PROCESS_CLOSED);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GamepadService::Stop, base::Unretained(this), source));
}

} // namespace content
