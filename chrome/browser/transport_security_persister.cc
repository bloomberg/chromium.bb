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
  : ALLOW_THIS_IN_INITIALIZER_LIST(save_coalescer_(this)) {
}

TransportSecurityPersister::~TransportSecurityPersister() {
  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::Initialize(
    net::TransportSecurityState* state, const FilePath& profile_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  transport_security_state_ = state;
  state_file_ =
      profile_path.Append(FILE_PATH_LITERAL("TransportSecurity"));
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this,
      &TransportSecurityPersister::Load);
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, task, 1000);
}

void TransportSecurityPersister::Load() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  std::string state;
  if (!file_util::ReadFileToString(state_file_, &state))
    return;

  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &TransportSecurityPersister::CompleteLoad,
                        state));
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  bool dirty = false;
  if (!transport_security_state_->Deserialise(state, &dirty)) {
    LOG(ERROR) << "Failed to deserialize state: " << state;
    return;
  }
  if (dirty)
    StateIsDirty(transport_security_state_);
}

void TransportSecurityPersister::StateIsDirty(
    net::TransportSecurityState* state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(state == transport_security_state_);

  if (!save_coalescer_.empty())
    return;

  Task* task = save_coalescer_.NewRunnableMethod(
      &TransportSecurityPersister::Save);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 1000);
}

void TransportSecurityPersister::Save() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  std::string state;
  if (!transport_security_state_->Serialise(&state))
    return;

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &TransportSecurityPersister::CompleteSave,
                        state));
}

void TransportSecurityPersister::CompleteSave(const std::string& state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  file_util::WriteFile(state_file_, state.data(), state.size());
}
