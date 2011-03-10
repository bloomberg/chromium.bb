// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_
#pragma once

#include <string>

#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/time.h"
#include "third_party/cros/chromeos_update_engine.h"

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
    virtual void UpdateStatusChanged(UpdateLibrary* library) = 0;
  };

//  static UpdateLibrary* GetStubImplementation();

  virtual ~UpdateLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Requests an update check and calls |callback| when completed.
  virtual void RequestUpdateCheck(chromeos::UpdateCallback callback,
                                  void* user_data) = 0;

  // Reboots if update has been performed.
  virtual bool RebootAfterUpdate() = 0;

  // Sets the release track (channel). |track| should look like
  // "beta-channel" and "dev-channel". Returns true on success.
  virtual bool SetReleaseTrack(const std::string& track) = 0;

  // Returns the release track (channel). On error, returns an empty
  // string.
  virtual std::string GetReleaseTrack() = 0;

  virtual const Status& status() const = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static UpdateLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_UPDATE_LIBRARY_H_
