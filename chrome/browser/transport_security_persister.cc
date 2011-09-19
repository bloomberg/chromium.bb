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

TransportSecurityPersister::TransportSecurityPersister(
    net::TransportSecurityState* state,
    const FilePath& profile_path,
    bool readonly)
    : transport_security_state_(state),
      writer_(profile_path.AppendASCII("TransportSecurity"),
              BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)),
      readonly_(readonly) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  transport_security_state_->SetDelegate(this);
}

TransportSecurityPersister::~TransportSecurityPersister() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();

  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &TransportSecurityPersister::Load));
}

void TransportSecurityPersister::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string state;
  if (!file_util::ReadFileToString(writer_.path(), &state))
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
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
  DCHECK_EQ(transport_security_state_, state);

  if (!readonly_)
    writer_.ScheduleWrite(this);
}

bool TransportSecurityPersister::SerializeData(std::string* data) {
  return transport_security_state_->Serialise(data);
}
