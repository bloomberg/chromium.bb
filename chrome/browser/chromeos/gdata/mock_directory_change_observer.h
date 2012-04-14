// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DIRECTORY_CHANGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DIRECTORY_CHANGE_OBSERVER_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gdata {

// Mock for GDataFileSystemInterface::Observer::OnDirectoryChanged().
class MockDirectoryChangeObserver : public GDataFileSystemInterface::Observer {
 public:
  MockDirectoryChangeObserver();
  virtual ~MockDirectoryChangeObserver();

  // GDataFileSystemInterface::Observer overrides.
  MOCK_METHOD1(OnDirectoryChanged, void(const FilePath& directory_path));
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DIRECTORY_CHANGE_OBSERVER_H_
