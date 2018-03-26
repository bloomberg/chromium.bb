// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_DELEGATE_H_

namespace content {

// BrowserThread::SetDelegate was deprecated, this is now only used by
// BrowserThread::SetIOThreadDelegate.
//
// When registered as such, it will schedule to run Init() before the message
// loop begins and receive a CleanUp call right after the message loop ends (and
// before the BrowserThread has done its own clean-up).
class BrowserThreadDelegate {
 public:
  virtual ~BrowserThreadDelegate() = default;

  // Called prior to starting the message loop
  virtual void Init() = 0;

  // Called just after the message loop ends.
  virtual void CleanUp() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_DELEGATE_H_
