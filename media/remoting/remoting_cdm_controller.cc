// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_cdm_controller.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"

namespace media {
namespace remoting {

RemotingCdmController::RemotingCdmController(
    scoped_refptr<SharedSession> session)
    : session_(std::move(session)) {
  session_->AddClient(this);
}

RemotingCdmController::~RemotingCdmController() {
  DCHECK(thread_checker_.CalledOnValidThread());

  session_->RemoveClient(this);
}

void RemotingCdmController::OnStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(!cdm_check_cb_.is_null());
  is_remoting_ = success;
  base::ResetAndReturn(&cdm_check_cb_).Run(success);
}

void RemotingCdmController::OnSessionStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_remoting_ && session_->state() == SharedSession::SESSION_STOPPING) {
    session_->Shutdown();
    is_remoting_ = false;
  }
}

void RemotingCdmController::ShouldCreateRemotingCdm(
    const CdmCheckCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!cb.is_null());

  if (is_remoting_) {
    cb.Run(true);
    return;
  }

  if (!session_->is_remote_decryption_available()) {
    cb.Run(false);
    return;
  }

  DCHECK(cdm_check_cb_.is_null());
  cdm_check_cb_ = cb;
  session_->StartRemoting(this);
}

}  // namespace remoting
}  // namespace media
