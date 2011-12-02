// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

GamepadService::GamepadService() : num_readers_(0) {
}

GamepadService::~GamepadService() {
}

GamepadService* GamepadService::GetInstance() {
  return Singleton<GamepadService>::get();
}

void GamepadService::Start(
    GamepadDataFetcher* data_fetcher,
    content::RenderProcessHost* associated_rph) {
  num_readers_++;
  if (!provider_)
    provider_ = new GamepadProvider(data_fetcher);
  DCHECK(num_readers_ > 0);
  provider_->Resume();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GamepadService::RegisterForCloseNotification,
                 base::Unretained(this),
                 associated_rph));
}

void GamepadService::RegisterForCloseNotification(
    content::RenderProcessHost* rph) {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::Source<content::RenderProcessHost>(rph));
}

base::SharedMemoryHandle GamepadService::GetSharedMemoryHandle(
    base::ProcessHandle handle) {
  return provider_->GetRendererSharedMemoryHandle(handle);
}

void GamepadService::Stop() {
  --num_readers_;
  DCHECK(num_readers_ >= 0);

  if (num_readers_ == 0)
    provider_->Pause();
}

void GamepadService::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GamepadService::Stop, base::Unretained(this)));
}

} // namespace content
