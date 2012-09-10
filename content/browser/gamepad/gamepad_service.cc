// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

GamepadService::GamepadService() : num_readers_(0) {
}

GamepadService::GamepadService(scoped_ptr<GamepadDataFetcher> fetcher)
    : num_readers_(0),
      provider_(new GamepadProvider(fetcher.Pass())) {
  thread_checker_.DetachFromThread();
}

GamepadService::~GamepadService() {
}

GamepadService* GamepadService::GetInstance() {
  return Singleton<GamepadService,
                   LeakySingletonTraits<GamepadService> >::get();
}

void GamepadService::AddConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  num_readers_++;
  DCHECK(num_readers_ > 0);
  if (!provider_.get())
    provider_.reset(new GamepadProvider);
  provider_->Resume();
}

void GamepadService::RemoveConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  --num_readers_;
  DCHECK(num_readers_ >= 0);

  if (num_readers_ == 0)
    provider_->Pause();
}

void GamepadService::RegisterForUserGesture(const base::Closure& closure) {
  DCHECK(num_readers_ > 0);
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_->RegisterForUserGesture(closure);
}

void GamepadService::Terminate() {
  provider_.reset();
}

base::SharedMemoryHandle GamepadService::GetSharedMemoryHandleForProcess(
    base::ProcessHandle handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return provider_->GetSharedMemoryHandleForProcess(handle);
}

}  // namespace content
