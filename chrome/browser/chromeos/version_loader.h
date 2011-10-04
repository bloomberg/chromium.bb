// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "content/browser/cancelable_request.h"

class FilePath;

namespace chromeos {

// ChromeOSVersionLoader loads the version of Chrome OS from the file system.
// Loading is done asynchronously on the file thread. Once loaded,
// ChromeOSVersionLoader callback to a method of your choice with the version
// (or an empty string if the version couldn't be found).
// To use ChromeOSVersionLoader do the following:
//
// . In your class define a member field of type chromeos::VersionLoader and
//   CancelableRequestConsumerBase.
// . Define the callback method, something like:
//   void OnGetChromeOSVersion(chromeos::VersionLoader::Handle,
//                             std::string version);
// . When you want the version invoke:  loader.GetVersion(&consumer, callback);
//
// This class also provides the ability to load the bios firmware using
//   loader.GetFirmware(&consumer, callback);
class VersionLoader : public CancelableRequestProvider {
 public:
  VersionLoader();
  virtual ~VersionLoader();

  enum VersionFormat {
    VERSION_SHORT,
    VERSION_SHORT_WITH_DATE,
    VERSION_FULL,
  };

  // Signature
  typedef base::Callback<void(Handle, std::string)> GetVersionCallback;
  typedef CancelableRequest<GetVersionCallback> GetVersionRequest;

  typedef base::Callback<void(Handle, std::string)> GetFirmwareCallback;
  typedef CancelableRequest<GetFirmwareCallback> GetFirmwareRequest;

  // Asynchronously requests the version.
  // If |full_version| is true version string with extra info is extracted,
  // otherwise it's in short format x.x.xx.x.
  Handle GetVersion(CancelableRequestConsumerBase* consumer,
                    const GetVersionCallback& callback,
                    VersionFormat format);

  Handle GetFirmware(CancelableRequestConsumerBase* consumer,
                     const GetFirmwareCallback& callback);

  // Parse the version information as a Chrome platfrom, not Chrome OS
  // TODO(rkc): Change this and everywhere it is used once we switch Chrome OS
  // over to xx.yyy.zz version numbers instead of 0.xx.yyy.zz
  // Refer to http://code.google.com/p/chromium-os/issues/detail?id=15789
  void EnablePlatformVersions(bool enable);

  static const char kFullVersionPrefix[];
  static const char kVersionPrefix[];
  static const char kFirmwarePrefix[];

 private:
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseFullVersion);
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseVersion);
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseFirmware);

  // VersionLoader calls into the Backend on the file thread to load
  // and extract the version.
  class Backend : public base::RefCountedThreadSafe<Backend> {
   public:
    Backend() {}

    // Calls ParseVersion to get the version # and notifies request.
    // This is invoked on the file thread.
    // If |full_version| is true then extra info is passed in version string.
    void GetVersion(scoped_refptr<GetVersionRequest> request,
                    VersionFormat format);

    // Calls ParseFirmware to get the firmware # and notifies request.
    // This is invoked on the file thread.
    void GetFirmware(scoped_refptr<GetFirmwareRequest> request);

   private:
    friend class base::RefCountedThreadSafe<Backend>;

    ~Backend() {}

    DISALLOW_COPY_AND_ASSIGN(Backend);
  };

  // Extracts the version from the file.
  // |prefix| specifies what key defines version data.
  static std::string ParseVersion(const std::string& contents,
                                  const std::string& prefix);

  // Extracts the firmware from the file.
  static std::string ParseFirmware(const std::string& contents);

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(VersionLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
