// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/strict_transport_security_persister.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/strict_transport_security_state.h"

StrictTransportSecurityPersister::StrictTransportSecurityPersister(
    net::StrictTransportSecurityState* state,
    base::Thread* file_thread,
    const FilePath& profile_path)
    : state_is_dirty_(false),
      strict_transport_security_state_(state),
      file_thread_(file_thread),
      state_file_(profile_path.Append(
          FILE_PATH_LITERAL("StrictTransportSecurity"))) {
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this,
      &StrictTransportSecurityPersister::LoadState);
  file_thread->message_loop()->PostDelayedTask(FROM_HERE, task,
                                               1000 /* 1 second */);
}

void StrictTransportSecurityPersister::LoadState() {
  // Runs on |file_thread_|
  AutoLock locked_(lock_);
  DCHECK(file_thread_->message_loop() == MessageLoop::current());

  std::string state;
  if (!file_util::ReadFileToString(state_file_, &state))
    return;

  strict_transport_security_state_->Deserialise(state);
}

void StrictTransportSecurityPersister::StateIsDirty(
    net::StrictTransportSecurityState* state) {
  // Runs on arbitary thread, may not block nor reenter
  // |strict_transport_security_state_|.
  AutoLock locked_(lock_);
  DCHECK(state == strict_transport_security_state_);

  if (state_is_dirty_)
    return;  // we already have a serialisation scheduled

  Task* task = NewRunnableMethod(this,
      &StrictTransportSecurityPersister::SerialiseState);
  file_thread_->message_loop()->PostDelayedTask(FROM_HERE, task,
                                                1000 /* 1 second */);
  state_is_dirty_ = true;
}

void StrictTransportSecurityPersister::SerialiseState() {
  // Runs on |file_thread_|
  AutoLock locked_(lock_);
  DCHECK(file_thread_->message_loop() == MessageLoop::current());

  DCHECK(state_is_dirty_);
  state_is_dirty_ = false;

  std::string state;
  if (!strict_transport_security_state_->Serialise(&state))
    return;

  file_util::WriteFile(state_file_, state.data(), state.size());
}
