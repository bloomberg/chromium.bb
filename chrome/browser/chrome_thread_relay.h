// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ChromeThreadRelay provides a convenient way to bounce work to a specific
// ChromeThread and then return results back to the originating thread.
//
// EXAMPLE:
//
//  class MyRelay : public ChromeThreadRelay {
//   public:
//    MyRelay(const Params& params) : params_(params) {
//    }
//   protected:
//    virtual void RunWork() {
//      results_ = DoWork(params_);
//    }
//    virtual void RunCallback() {
//      ... use results_ on the originating thread ...
//    }
//   private:
//    Params params_;
//    Results_ results_;
//  };

#ifndef CHROME_BROWSER_CHROME_THREAD_RELAY_H_
#define CHROME_BROWSER_CHROME_THREAD_RELAY_H_

#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"

class ChromeThreadRelay
    : public base::RefCountedThreadSafe<ChromeThreadRelay> {
 public:
  ChromeThreadRelay();

  void Start(ChromeThread::ID target_thread_id,
             const tracked_objects::Location& from_here);

 protected:
  friend class base::RefCountedThreadSafe<ChromeThreadRelay>;
  virtual ~ChromeThreadRelay() {}

  // Called to perform work on the FILE thread.
  virtual void RunWork() = 0;

  // Called to notify the callback on the origin thread.
  virtual void RunCallback() = 0;

 private:
  void ProcessOnTargetThread();
  ChromeThread::ID origin_thread_id_;
};

#endif  // CHROME_BROWSER_CHROME_THREAD_RELAY_H_
