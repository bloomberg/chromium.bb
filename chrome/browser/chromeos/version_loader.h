// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
class VersionLoader : public CancelableRequestProvider {
 public:
  VersionLoader();

  enum VersionFormat {
    VERSION_SHORT,
    VERSION_SHORT_WITH_DATE,
    VERSION_FULL,
  };

  // Signature
  typedef Callback2<Handle, std::string>::Type GetVersionCallback;

  typedef CancelableRequest<GetVersionCallback> GetVersionRequest;

  // Asynchronously requests the version.
  // If |full_version| is true version string with extra info is extracted,
  // otherwise it's in short format x.x.xx.x.
  Handle GetVersion(CancelableRequestConsumerBase* consumer,
                    GetVersionCallback* callback,
                    VersionFormat format);

  static const char kFullVersionPrefix[];
  static const char kVersionPrefix[];

 private:
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseFullVersion);
  FRIEND_TEST_ALL_PREFIXES(VersionLoaderTest, ParseVersion);

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

   private:
    friend class base::RefCountedThreadSafe<Backend>;

    ~Backend() {}

    DISALLOW_COPY_AND_ASSIGN(Backend);
  };

  // Extracts the version from the file.
  // |prefix| specifies what key defines version data.
  static std::string ParseVersion(const std::string& contents,
                                  const std::string& prefix);

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(VersionLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VERSION_LOADER_H_
