// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/strict_transport_security_persister.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/strict_transport_security_state.h"

StrictTransportSecurityPersister::StrictTransportSecurityPersister()
    : state_is_dirty_(false) {
}

StrictTransportSecurityPersister::~StrictTransportSecurityPersister() {
  strict_transport_security_state_->SetDelegate(NULL);
}

void StrictTransportSecurityPersister::Initialize(
    net::StrictTransportSecurityState* state, const FilePath& profile_path) {
  strict_transport_security_state_ = state;
  state_file_ =
      profile_path.Append(FILE_PATH_LITERAL("StrictTransportSecurity"));
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this,
      &StrictTransportSecurityPersister::LoadState);
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, task, 1000);
}

void StrictTransportSecurityPersister::LoadState() {
  AutoLock locked_(lock_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

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
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, task, 1000);
  state_is_dirty_ = true;
}

void StrictTransportSecurityPersister::SerialiseState() {
  AutoLock locked_(lock_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  DCHECK(state_is_dirty_);
  state_is_dirty_ = false;

  std::string state;
  if (!strict_transport_security_state_->Serialise(&state))
    return;

  file_util::WriteFile(state_file_, state.data(), state.size());
}
