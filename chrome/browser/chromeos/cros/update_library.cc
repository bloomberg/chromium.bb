// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/update_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace chromeos {

class UpdateLibraryImpl : public UpdateLibrary {
 public:
  UpdateLibraryImpl()
    : status_connection_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      Init();
    }
  }

  ~UpdateLibraryImpl() {
    if (status_connection_) {
      DisconnectUpdateProgress(status_connection_);
    }
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(Observer* observer) {
    return observers_.HasObserver(observer);
  }

  void RequestUpdateCheck(chromeos::UpdateCallback callback, void* user_data) {
    if (CrosLibrary::Get()->EnsureLoaded())
      chromeos::RequestUpdateCheck(callback, user_data);
  }

  bool RebootAfterUpdate() {
    if (!CrosLibrary::Get()->EnsureLoaded())
      return false;

    return RebootIfUpdated();
  }

  void SetReleaseTrack(const std::string& track) {
    if (CrosLibrary::Get()->EnsureLoaded())
      chromeos::SetUpdateTrack(track);
  }

  void GetReleaseTrack(chromeos::UpdateTrackCallback callback,
                       void* user_data) {
    if (CrosLibrary::Get()->EnsureLoaded())
      chromeos::RequestUpdateTrack(callback, user_data);
  }

  const UpdateLibrary::Status& status() const {
    return status_;
  }

 private:
  static void ChangedHandler(void* object,
      const UpdateProgress& status) {
    UpdateLibraryImpl* updater = static_cast<UpdateLibraryImpl*>(object);
    updater->UpdateStatus(Status(status));
  }

  void Init() {
    status_connection_ = MonitorUpdateStatus(&ChangedHandler, this);
  }

  void UpdateStatus(const Status& status) {
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &UpdateLibraryImpl::UpdateStatus, status));
      return;
    }

    status_ = status;
    FOR_EACH_OBSERVER(Observer, observers_, UpdateStatusChanged(this));

    // If the update is ready to install, send a notification so that Chrome
    // can update the UI.
    if (status_.status == UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      NotificationService::current()->Notify(
          NotificationType::UPGRADE_RECOMMENDED,
          Source<UpdateLibrary>(this),
          NotificationService::NoDetails());
    }
  }

  ObserverList<Observer> observers_;

  // A reference to the update api, to allow callbacks when the update
  // status changes.
  UpdateStatusConnection status_connection_;

  // The latest power status.
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(UpdateLibraryImpl);
};

class UpdateLibraryStubImpl : public UpdateLibrary {
 public:
  UpdateLibraryStubImpl() {}
  ~UpdateLibraryStubImpl() {}
  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}
  bool HasObserver(Observer* observer) { return false; }
  void RequestUpdateCheck(chromeos::UpdateCallback callback, void* user_data) {
    if (callback)
      callback(user_data, UPDATE_RESULT_FAILED, "stub update");
  }
  bool RebootAfterUpdate() { return false; }
  void SetReleaseTrack(const std::string& track) { }
  void GetReleaseTrack(chromeos::UpdateTrackCallback callback,
                       void* user_data) {
    if (callback)
      callback(user_data, "beta-channel");
  }
  const UpdateLibrary::Status& status() const {
    return status_;
  }

 private:
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(UpdateLibraryStubImpl);
};

// static
UpdateLibrary* UpdateLibrary::GetImpl(bool stub) {
  if (stub)
    return new UpdateLibraryStubImpl();
  else
    return new UpdateLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::UpdateLibraryImpl);
