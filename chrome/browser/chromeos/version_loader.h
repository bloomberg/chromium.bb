// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/task/cancelable_task_tracker.h"

namespace chromeos {

// ChromeOSVersionLoader loads the version of Chrome OS from the file system.
// Loading is done asynchronously in the blocking thread pool. Once loaded,
// ChromeOSVersionLoader callback to a method of your choice with the version
// (or an empty string if the version couldn't be found).
// To use ChromeOSVersionLoader do the following:
//
// . In your class define a member field of type chromeos::VersionLoader and
//   base::CancelableTaskTracker.
// . Define the callback method, something like:
//   void OnGetChromeOSVersion(const std::string& version);
// . When you want the version invoke:
//   VersionLoader::GetVersion()
//
// This class also provides the ability to load the bios firmware using
//   VersionLoader::GetFirmware()
class VersionLoader {
 public:
  VersionLoader();
  virtual ~VersionLoader();

  enum VersionFormat {
    VERSION_SHORT,
    VERSION_SHORT_WITH_DATE,
    VERSION_FULL,
  };

  // Signature
  typedef base::Callback<void(const std::string&)> GetVersionCallback;
  typedef base::Callback<void(const std::string&)> GetFirmwareCallback;

  // Asynchronously requests the version.
  // If |full_version| is true version string with extra info is extracted,
  // otherwise it's in short format x.x.xx.x.
  base::CancelableTaskTracker::TaskId GetVersion(
      VersionFormat format,
      const GetVersionCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetFirmware(
      const GetFirmwareCallback& callback,
      base::CancelableTaskTracker* tracker);

 private:
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseFirmware);

  // VersionLoader calls into the Backend in the blocking thread pool to load
  // and extract the version.
  class Backend : public base::RefCountedThreadSafe<Backend> {
   public:
    Backend() {}

    // Gets the version number from base::SysInfo. This is invoked on the
    // blocking thread pool.
    void GetVersion(VersionFormat format, std::string* version);

    // Calls ParseFirmware to get the firmware value. This is invoked on the
    // blocking thread pool.
    void GetFirmware(std::string* firmware);

   private:
    friend class base::RefCountedThreadSafe<Backend>;

    ~Backend() {}

    DISALLOW_COPY_AND_ASSIGN(Backend);
  };

  // Extracts the firmware from the file.
  static std::string ParseFirmware(const std::string& contents);

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(VersionLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
