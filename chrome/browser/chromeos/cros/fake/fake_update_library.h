// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_UPDATE_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_UPDATE_LIBRARY_H_

#include "chrome/browser/chromeos/cros/update_library.h"

namespace chromeos {

class FakeUpdateLibrary : public UpdateLibrary {
 public:
  FakeUpdateLibrary() {}
  virtual ~FakeUpdateLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual const Status& status() const {
    return status_;
  }

 private:
  Status status_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_UPDATE_LIBRARY_H_
