// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/drive/file_cache.h"

namespace drive {

// This class is used to report fake free disk space. In particular, this
// class can be used to simulate a case where disk is full, or nearly full.
class FakeFreeDiskSpaceGetter : public FreeDiskSpaceGetterInterface {
 public:
  FakeFreeDiskSpaceGetter();
  virtual ~FakeFreeDiskSpaceGetter();

  // If this function is not called, AmountOfFreeDiskSpace() will return 0
  // repeatedly.
  //
  // If this function is only called once, AmountOfFreeDiskSpace() will
  // return the fake value repeatedly.
  //
  // If this function is called multiple times, AmountOfFreeDiskSpace() will
  // return the fake values in the same order these values were recorded. The
  // last value will be returned repeatedly.
  void set_fake_free_disk_space(int64 fake_free_disk_space) {
    fake_values_.push_back(fake_free_disk_space);
  }

  // FreeDiskSpaceGetterInterface overrides.
  virtual int64 AmountOfFreeDiskSpace() OVERRIDE;

 private:
  std::vector<int64> fake_values_;

  DISALLOW_COPY_AND_ASSIGN(FakeFreeDiskSpaceGetter);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_
