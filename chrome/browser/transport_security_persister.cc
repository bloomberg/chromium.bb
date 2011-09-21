// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/transport_security_persister.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "net/base/transport_security_state.h"

class TransportSecurityPersister::Loader {
 public:
  Loader(const base::WeakPtr<TransportSecurityPersister>& persister,
         const FilePath& path)
      : persister_(persister),
        path_(path),
        state_valid_(false) {
  }

  void Load() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    state_valid_ = file_util::ReadFileToString(path_, &state_);
  }

  void CompleteLoad() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // Make sure we're deleted.
    scoped_ptr<Loader> deleter(this);

    if (!persister_ || !state_valid_)
      return;
    persister_->CompleteLoad(state_);
  }

 private:
  base::WeakPtr<TransportSecurityPersister> persister_;

  FilePath path_;

  std::string state_;
  bool state_valid_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

TransportSecurityPersister::TransportSecurityPersister(
    net::TransportSecurityState* state,
    const FilePath& profile_path,
    bool readonly)
    : transport_security_state_(state),
      writer_(profile_path.AppendASCII("TransportSecurity"),
              BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)),
      readonly_(readonly),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  transport_security_state_->SetDelegate(this);

  Loader* loader = new Loader(weak_ptr_factory_.GetWeakPtr(), writer_.path());
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&Loader::Load, base::Unretained(loader)),
      base::Bind(&Loader::CompleteLoad, base::Unretained(loader)));
}

TransportSecurityPersister::~TransportSecurityPersister() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();

  transport_security_state_->SetDelegate(NULL);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return transport_security_state_->Serialise(data);
}
