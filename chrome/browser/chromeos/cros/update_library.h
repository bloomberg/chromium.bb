// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_

#include <string>

#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/time.h"
#include "cros/chromeos_update_engine.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS update library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: chromeos::CrosLibrary::Get()->GetUpdateLibrary()

class UpdateLibrary {
 public:
  // TODO(seanparent): Should make the UpdateProgress type copyable.
  // We need to copy it to bind it for a deferred notification.
  // Modifying the cros library just for that, for a single use case,
  // isn't worth it. Instead we define this a local Status struct that
  // is copyable.

  struct Status {
    Status()
        : status(UPDATE_STATUS_IDLE),
          download_progress(0.0),
          last_checked_time(0),
          new_size(0) {
    }

    explicit Status(const UpdateProgress& x) :
        status(x.status_),
        download_progress(x.download_progress_),
        last_checked_time(x.last_checked_time_),
        new_version(x.new_version_),
        new_size(x.new_size_) {
    }

    UpdateStatusOperation status;
    double download_progress;  // 0.0 - 1.0
    int64_t last_checked_time;  // As reported by std::time().
    std::string new_version;
    int64_t new_size;  // Valid during DOWNLOADING, in bytes.
  };

  class Observer {
   public:
    virtual ~Observer() { }
    virtual void Changed(UpdateLibrary* obj) = 0;
  };

  virtual ~UpdateLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const Status& status() const = 0;
};

class UpdateLibraryImpl : public UpdateLibrary {
 public:
  UpdateLibraryImpl();
  virtual ~UpdateLibraryImpl();

  // UpdateLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  virtual const Status& status() const;

 private:

  // This method is called when there's a change in status.
  // This method is called on a background thread.
  static void ChangedHandler(void* object, const UpdateProgress& status);

  // This methods starts the monitoring of power changes.
  void Init();

  // Called by the handler to update the power status.
  // This will notify all the Observers.
  void UpdateStatus(const Status& status);

  ObserverList<Observer> observers_;

  // A reference to the update api, to allow callbacks when the update
  // status changes.
  UpdateStatusConnection status_connection_;

  // The latest power status.
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(UpdateLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_

