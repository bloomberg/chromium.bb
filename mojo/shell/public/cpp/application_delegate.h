// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APPLICATION_DELEGATE_H_
#define MOJO_SHELL_PUBLIC_CPP_APPLICATION_DELEGATE_H_

#include <string>

#include "mojo/public/cpp/system/macros.h"

namespace mojo {

class ApplicationConnection;
class ApplicationImpl;

// An abstract class that the application may subclass to control various
// behaviors of ApplicationImpl.
class ApplicationDelegate {
 public:
  ApplicationDelegate();
  virtual ~ApplicationDelegate();

  // Called exactly once before any other method.
  virtual void Initialize(ApplicationImpl* app);

  // Override this method to configure what services a connection supports when
  // being connected to from an app.
  // Return false to reject the connection entirely.
  virtual bool AcceptConnection(ApplicationConnection* connection);

  // Called when the shell connection has a connection error.
  //
  // Return true to shutdown the application. Return false to skip shutting
  // down the connection, but user is then required to call
  // ApplicationImpl::QuitNow() when done.
  virtual bool ShellConnectionLost();

  // Called before ApplicationImpl::Terminate(). After returning from this call
  // the delegate can no longer rely on the main run loop still running.
  virtual void Quit();

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationDelegate);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APPLICATION_DELEGATE_H_
