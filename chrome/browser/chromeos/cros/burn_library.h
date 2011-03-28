// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "third_party/cros/chromeos_imageburn.h"

struct ImageBurnStatus {
  explicit ImageBurnStatus(const chromeos::BurnStatus& status)
      : amount_burnt(status.amount_burnt),
        total_size(status.total_size) {
    if (status.target_path)
      target_path = status.target_path;
    if (status.error)
      error = status.error;
  }
  std::string target_path;
  int64 amount_burnt;
  int64 total_size;
  std::string error;
};

namespace chromeos {
class BurnLibrary {
 public:
  class Observer {
   public:
    virtual void ProgressUpdated(BurnLibrary* object, BurnEventType evt,
                                 const ImageBurnStatus& status) = 0;
  };

  virtual ~BurnLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool DoBurn(const FilePath& from_path, const FilePath& to_path) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static BurnLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_BURN_LIBRARY_H_
