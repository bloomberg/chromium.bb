// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOOT_TIMES_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_BOOT_TIMES_LOADER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/cancelable_request.h"

namespace chromeos {

// BootTimesLoader loads the bootimes of Chrome OS from the file system.
// Loading is done asynchronously on the file thread. Once loaded,
// BootTimesLoader calls back to a method of your choice with the boot times.
// To use BootTimesLoader do the following:
//
// . In your class define a member field of type chromeos::BootTimesLoader and
//   CancelableRequestConsumerBase.
// . Define the callback method, something like:
//   void OnBootTimesLoader(chromeos::BootTimesLoader::Handle,
//                             BootTimesLoader::BootTimes boot_times);
// . When you want the version invoke: loader.GetBootTimes(&consumer, callback);
class BootTimesLoader : public CancelableRequestProvider {
 public:
  BootTimesLoader();

  // All fields are 0.0 if they couldn't be found.
  typedef struct BootTimes {
    double firmware;           // Time from power button to kernel being loaded.
    double pre_startup;        // Time from kernel to system code being called.
    double x_started;          // Time X server is ready to be connected to.
    double login_prompt_ready; // Time login (or OOB) panel is displayed.

    BootTimes() : firmware(0),
                  pre_startup(0),
                  x_started(0),
                  login_prompt_ready(0) {}
  } BootTimes;

  // Signature
  typedef Callback2<Handle, BootTimes>::Type GetBootTimesCallback;

  typedef CancelableRequest<GetBootTimesCallback> GetBootTimesRequest;

  // Asynchronously requests the info.
  Handle GetBootTimes(
      CancelableRequestConsumerBase* consumer,
      GetBootTimesCallback* callback);

 private:
  // BootTimesLoader calls into the Backend on the file thread to load
  // and extract the boot times.
  class Backend : public base::RefCountedThreadSafe<Backend> {
   public:
    Backend() {}

    void GetBootTimes(scoped_refptr<GetBootTimesRequest> request);

   private:
    friend class base::RefCountedThreadSafe<Backend>;

    ~Backend() {}

    DISALLOW_COPY_AND_ASSIGN(Backend);
  };

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(BootTimesLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BOOT_TIMES_LOADER_H_
