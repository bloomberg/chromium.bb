// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FREE_DISK_SPACE_GETTER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FREE_DISK_SPACE_GETTER_H_

#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drive {

class MockFreeDiskSpaceGetter : public FreeDiskSpaceGetterInterface {
   public:
     MockFreeDiskSpaceGetter();
     virtual ~MockFreeDiskSpaceGetter();
     MOCK_CONST_METHOD0(AmountOfFreeDiskSpace, int64());
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_FREE_DISK_SPACE_GETTER_H_
