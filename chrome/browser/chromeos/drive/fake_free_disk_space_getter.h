// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_

#include <list>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/drive/file_cache.h"

namespace drive {

// This class is used to report fake free disk space. In particular, this
// class can be used to simulate a case where disk is full, or nearly full.
class FakeFreeDiskSpaceGetter : public internal::FreeDiskSpaceGetterInterface {
 public:
  FakeFreeDiskSpaceGetter();
  virtual ~FakeFreeDiskSpaceGetter();

  void set_default_value(int64 value) { default_value_ = value; }

  // Pushes the given value to the back of the fake value list.
  //
  // If the fake value list is empty, AmountOfFreeDiskSpace() will return
  // |default_value_| repeatedly.
  // Otherwise, AmountOfFreeDiskSpace() will return the value at the front of
  // the list and removes it from the list.
  void PushFakeValue(int64 value);

  // FreeDiskSpaceGetterInterface overrides.
  virtual int64 AmountOfFreeDiskSpace() OVERRIDE;

 private:
  std::list<int64> fake_values_;
  int64 default_value_;

  DISALLOW_COPY_AND_ASSIGN(FakeFreeDiskSpaceGetter);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FAKE_FREE_DISK_SPACE_GETTER_H_
