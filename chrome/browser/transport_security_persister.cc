// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/transport_security_persister.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "net/base/transport_security_state.h"

TransportSecurityPersister::TransportSecurityPersister(bool readonly)
  : ALLOW_THIS_IN_INITIALIZER_LIST(save_coalescer_(this)),
    readonly_(readonly) {
}

TransportSecurityPersister::~TransportSecurityPersister() {
  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::Initialize(
    net::TransportSecurityState* state, const FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  transport_security_state_ = state;
  state_file_ =
      profile_path.Append(FILE_PATH_LITERAL("TransportSecurity"));
  state->SetDelegate(this);

  Task* task = NewRunnableMethod(this,
      &TransportSecurityPersister::Load);
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE, task, 1000);
}

void TransportSecurityPersister::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string state;
  if (!file_util::ReadFileToString(state_file_, &state))
    return;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &TransportSecurityPersister::CompleteLoad,
                        state));
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool dirty = false;
  if (!transport_security_state_->LoadEntries(state, &dirty)) {
    LOG(ERROR) << "Failed to deserialize state: " << state;
    return;
  }
  if (dirty)
    StateIsDirty(transport_security_state_);
}

void TransportSecurityPersister::StateIsDirty(
    net::TransportSecurityState* state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(state == transport_security_state_);

  if (readonly_)
    return;

  if (!save_coalescer_.empty())
    return;

  Task* task = save_coalescer_.NewRunnableMethod(
      &TransportSecurityPersister::Save);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 1000);
}

void TransportSecurityPersister::Save() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::string state;
  if (!transport_security_state_->Serialise(&state))
    return;

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &TransportSecurityPersister::CompleteSave,
                        state));
}

void TransportSecurityPersister::CompleteSave(const std::string& state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!readonly_);

  file_util::WriteFile(state_file_, state.data(), state.size());
}
