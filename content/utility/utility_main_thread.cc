// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_main_thread.h"

#include "content/child/child_process.h"
#include "content/utility/utility_thread_impl.h"

namespace content {

// We want to ensure there's only one utility thread running at a time, as there
// are many globals used in the utility process.
static base::LazyInstance<base::Lock> g_one_utility_thread_lock;

UtilityMainThread::UtilityMainThread(const std::string& channel_id)
    : Thread("Chrome_InProcUtilityThread"), channel_id_(channel_id) {
}

UtilityMainThread::~UtilityMainThread() {
  Stop();
}

void UtilityMainThread::Init() {
  // We need to return right away or else the main thread that started us will
  // hang.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&UtilityMainThread::InitInternal, base::Unretained(this)));
}

void UtilityMainThread::CleanUp() {
  child_process_.reset();

  // See comment in RendererMainThread.
  SetThreadWasQuitProperly(true);
  g_one_utility_thread_lock.Get().Release();
}

void UtilityMainThread::InitInternal() {
  g_one_utility_thread_lock.Get().Acquire();
  child_process_.reset(new ChildProcess());
  child_process_->set_main_thread(new UtilityThreadImpl(channel_id_));
}

base::Thread* CreateUtilityMainThread(const std::string& channel_id) {
  return new UtilityMainThread(channel_id);
}

}  // namespace content
