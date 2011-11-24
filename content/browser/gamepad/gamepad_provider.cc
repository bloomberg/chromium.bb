// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/browser/gamepad/data_fetcher.h"
#include "content/common/gamepad_messages.h"

#if defined(OS_WIN)
#include "content/browser/gamepad/data_fetcher_win.h"
#endif

namespace gamepad {

Provider* Provider::instance_ = NULL;

using namespace content;

// Define the default data fetcher that Provider will use if none is supplied.
// (PlatformDataFetcher).
#if defined(OS_WIN)

typedef DataFetcherWindows PlatformDataFetcher;

#else

class EmptyDataFetcher : public DataFetcher {
 public:
  void GetGamepadData(WebKit::WebGamepads* pads, bool) {
    pads->length = 0;
  }
};
typedef EmptyDataFetcher PlatformDataFetcher;

#endif

Provider::Provider(DataFetcher* fetcher)
    : creator_loop_(MessageLoop::current()->message_loop_proxy()),
      provided_fetcher_(fetcher),
      devices_changed_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  size_t data_size = sizeof(GamepadHardwareBuffer);
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);
  gamepad_shared_memory_.CreateAndMapAnonymous(data_size);
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  memset(hwbuf, 0, sizeof(GamepadHardwareBuffer));
}

Provider::~Provider() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);
  Stop();
}

base::SharedMemoryHandle Provider::GetRendererSharedMemoryHandle(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  gamepad_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void Provider::OnDevicesChanged() {
  devices_changed_ = true;
}

void Provider::Start() {
  DCHECK(MessageLoop::current()->message_loop_proxy() == creator_loop_);

  if (polling_thread_.get())
    return;

  polling_thread_.reset(new base::Thread("Gamepad polling thread"));
  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start gamepad polling thread";
    polling_thread_.reset();
    return;
  }

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&Provider::DoInitializePollingThread, this));
}

void Provider::Stop() {
  DCHECK(MessageLoop::current()->message_loop_proxy() == creator_loop_);

  polling_thread_.reset();
  data_fetcher_.reset();
}

void Provider::DoInitializePollingThread() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  if (!provided_fetcher_.get())
    provided_fetcher_.reset(new PlatformDataFetcher);

  // Pass ownership of fetcher to provider_.
  data_fetcher_.swap(provided_fetcher_);

  // Start polling.
  ScheduleDoPoll();
}

void Provider::DoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  base::subtle::Barrier_AtomicIncrement(&hwbuf->start_marker, 1);
  data_fetcher_->GetGamepadData(&hwbuf->buffer, devices_changed_);
  base::subtle::Barrier_AtomicIncrement(&hwbuf->end_marker, 1);
  devices_changed_ = false;
  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void Provider::ScheduleDoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Provider::DoPoll, weak_factory_.GetWeakPtr()),
      kDesiredSamplingIntervalMs);
}

GamepadHardwareBuffer* Provider::SharedMemoryAsHardwareBuffer() {
  void* mem = gamepad_shared_memory_.memory();
  DCHECK(mem);
  return static_cast<GamepadHardwareBuffer*>(mem);
}

}  // namespace gamepad
