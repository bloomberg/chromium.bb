// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_THREAD_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_THREAD_DELEGATE_H_

namespace web {

// A class with this type may be registered via WebThread::SetDelegate.
// TODO(stuartmorgan): Currently the above is not actually true; because
// web can currently be build with either its own thread implementation, or
// the content thread implementation, WebThread doesn't have SetDelegate
// (since it can't be easily be passed through to BrowserThread::SetDelegate).
// Once BrowserThread isn't being used by Chrome, SetDelegate will become
// public.
//
// If registered as such, it will schedule to run Init() before the
// message loop begins and the schedule InitAsync() as the first
// task on its message loop (after the WebThread has done its own
// initialization), and receive a CleanUp call right after the message
// loop ends (and before the WebThread has done its own clean-up).
class WebThreadDelegate {
 public:
  virtual ~WebThreadDelegate() {}

  // Called prior to starting the message loop
  virtual void Init() = 0;

  // Called as the first task on the thread's message loop.
  virtual void InitAsync() = 0;

  // Called just after the message loop ends.
  virtual void CleanUp() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_THREAD_DELEGATE_H_
