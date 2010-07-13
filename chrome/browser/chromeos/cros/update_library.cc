// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/update_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::UpdateLibraryImpl);

namespace chromeos {

UpdateLibraryImpl::UpdateLibraryImpl()
    : status_connection_(NULL) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    Init();
  }
}

UpdateLibraryImpl::~UpdateLibraryImpl() {
  if (status_connection_) {
    DisconnectUpdateProgress(status_connection_);
  }
}

void UpdateLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UpdateLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const UpdateLibrary::Status& UpdateLibraryImpl::status() const {
  return status_;
}

// static
void UpdateLibraryImpl::ChangedHandler(void* object,
    const UpdateProgress& status) {
  UpdateLibraryImpl* power = static_cast<UpdateLibraryImpl*>(object);
  power->UpdateStatus(Status(status));
}

void UpdateLibraryImpl::Init() {
  status_connection_ = MonitorUpdateStatus(&ChangedHandler, this);
}

void UpdateLibraryImpl::UpdateStatus(const Status& status) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &UpdateLibraryImpl::UpdateStatus, status));
    return;
  }

  status_ = status;
  FOR_EACH_OBSERVER(Observer, observers_, Changed(this));
}

}  // namespace chromeos

