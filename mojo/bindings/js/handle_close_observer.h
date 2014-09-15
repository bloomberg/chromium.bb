// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_BINDINGS_JS_HANDLE_CLOSE_OBSERVER_H_
#define MOJO_BINDINGS_JS_HANDLE_CLOSE_OBSERVER_H_

namespace gin {

class HandleCloseObserver {
 public:
  virtual void OnWillCloseHandle() = 0;

 protected:
  virtual ~HandleCloseObserver() {}
};

}  // namespace gin

#endif  // MOJO_BINDINGS_JS_HANDLE_CLOSE_OBSERVER_H_
