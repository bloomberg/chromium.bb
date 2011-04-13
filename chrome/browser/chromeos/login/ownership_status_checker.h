// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNERSHIP_STATUS_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNERSHIP_STATUS_CHECKER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/login/ownership_service.h"

namespace chromeos {

// A helper that does ownership checking on the file thread and reports back the
// result on whatever thread the call was made on. The pattern is to construct
// a checker, passing in the callback. Once the check is done, the callback will
// be invoked with the result. In order to cancel the callback, just destroy the
// checker object.
class OwnershipStatusChecker {
 public:
  // Callback function type. The status code is guaranteed to be different from
  // OWNERSHIP_UNKNOWN.
  typedef Callback1<OwnershipService::Status>::Type Callback;

  explicit OwnershipStatusChecker(Callback* callback);
  ~OwnershipStatusChecker();

 private:
  // The refcounted core that handles the thread switching.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    explicit Core(Callback* callback);

    // Starts the check.
    void Check();

    // Cancels the outstanding callback (if applicable).
    void Cancel();

   private:
    void CheckOnFileThread();
    void ReportResult(OwnershipService::Status status);

    scoped_ptr<Callback> callback_;
    scoped_refptr<base::MessageLoopProxy> origin_loop_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipStatusChecker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNERSHIP_STATUS_CHECKER_H_
