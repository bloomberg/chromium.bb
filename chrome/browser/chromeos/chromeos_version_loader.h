// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEOS_VERSION_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEOS_VERSION_LOADER_H_

#include "chrome/browser/cancelable_request.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class FilePath;

// ChromeOSVersionLoader loads the version of Chrome OS from the file system.
// Loading is done asynchronously on the file thread. Once loaded,
// ChromeOSVersionLoader callsback to a method of your choice with the version
// (or an empty string if the version couldn't be found).
// To use ChromeOSVersionLoader do the following:
//
// . In your class define a member field of type ChromeOSVersionLoader and
//   CancelableRequestConsumerBase.
// . Define the callback method, something like:
//   void OnGetChromeOSVersion(ChromeOSVersionLoader::Handle,
//                             std::string version);
// . When you want the version invoke:  loader.GetVersion(&consumer, callback);
class ChromeOSVersionLoader : public CancelableRequestProvider {
 public:
  ChromeOSVersionLoader();

  // Signature
  typedef Callback2<Handle, std::string>::Type GetVersionCallback;

  typedef CancelableRequest<GetVersionCallback> GetVersionRequest;

  // Asynchronously requests the version.
  Handle GetVersion(CancelableRequestConsumerBase* consumer,
                    GetVersionCallback* callback);

 private:
  FRIEND_TEST(ChromeOSVersionLoaderTest, ParseVersion);

  // ChromeOSVersionLoader calls into the Backend on the file thread to load
  // and extract the version.
  class Backend : public base::RefCountedThreadSafe<Backend> {
   public:
    Backend() {}

    // Calls ParseVersion to get the version # and notifies request.
    // This is invoked on the file thread.
    void GetVersion(scoped_refptr<GetVersionRequest> request);

   private:
    DISALLOW_COPY_AND_ASSIGN(Backend);
  };

  // Extracts the version from the file.
  static std::string ParseVersion(const std::string& contents);

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSVersionLoader);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEOS_VERSION_LOADER_H_
