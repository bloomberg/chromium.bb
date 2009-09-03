// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/force_tls_persister.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/force_tls_state.h"

ForceTLSPersister::ForceTLSPersister(net::ForceTLSState* state,
                                     base::Thread* file_thread)
    : state_is_dirty_(false),
      force_tls_state_(state),
      file_thread_(file_thread) {
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this, &ForceTLSPersister::LoadState);
  file_thread->message_loop()->PostDelayedTask(FROM_HERE, task,
                                               1000 /* 1 second */);
}

static FilePath GetStateFile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.Append("ForceTLSState");
}

void ForceTLSPersister::LoadState() {
  // Runs on |file_thread_|
  AutoLock locked_(lock_);
  DCHECK(file_thread_->message_loop() == MessageLoop::current());

  std::string state;
  if (!file_util::ReadFileToString(GetStateFile(), &state))
    return;

  force_tls_state_->Deserialise(state);
}

void ForceTLSPersister::StateIsDirty(net::ForceTLSState* state) {
  // Runs on arbitary thread, may not block nor reenter |force_tls_state_|
  AutoLock locked_(lock_);
  DCHECK(state == force_tls_state_);

  if (state_is_dirty_)
    return;  // we already have a serialisation scheduled

  Task* task = NewRunnableMethod(this, &ForceTLSPersister::SerialiseState);
  file_thread_->message_loop()->PostDelayedTask(FROM_HERE, task,
                                                1000 /* 1 second */);
  state_is_dirty_ = true;
}

void ForceTLSPersister::SerialiseState() {
  // Runs on |file_thread_|
  AutoLock locked_(lock_);
  DCHECK(file_thread_->message_loop() == MessageLoop::current());

  DCHECK(state_is_dirty_);
  state_is_dirty_ = false;

  std::string state;
  if (!force_tls_state_->Serialise(&state))
    return;

  file_util::WriteFile(GetStateFile(), state.data(), state.size());
}
