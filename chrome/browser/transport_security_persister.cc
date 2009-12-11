// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/transport_security_persister.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/transport_security_state.h"

TransportSecurityPersister::TransportSecurityPersister()
    : state_is_dirty_(false) {
}

TransportSecurityPersister::~TransportSecurityPersister() {
  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::Initialize(
    net::TransportSecurityState* state, const FilePath& profile_path) {
  transport_security_state_ = state;
  state_file_ =
      profile_path.Append(FILE_PATH_LITERAL("TransportSecurity"));
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this,
      &TransportSecurityPersister::LoadState);
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, task, 1000);
}

void TransportSecurityPersister::LoadState() {
  AutoLock locked_(lock_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  std::string state;
  if (!file_util::ReadFileToString(state_file_, &state))
    return;

  transport_security_state_->Deserialise(state);
}

void TransportSecurityPersister::StateIsDirty(
    net::TransportSecurityState* state) {
  // Runs on arbitary thread, may not block nor reenter
  // |transport_security_state_|.
  AutoLock locked_(lock_);
  DCHECK(state == transport_security_state_);

  if (state_is_dirty_)
    return;  // we already have a serialisation scheduled

  Task* task = NewRunnableMethod(this,
      &TransportSecurityPersister::SerialiseState);
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, task, 1000);
  state_is_dirty_ = true;
}

void TransportSecurityPersister::SerialiseState() {
  AutoLock locked_(lock_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  DCHECK(state_is_dirty_);
  state_is_dirty_ = false;

  std::string state;
  if (!transport_security_state_->Serialise(&state))
    return;

  file_util::WriteFile(state_file_, state.data(), state.size());
}
