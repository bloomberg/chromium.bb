// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APP_LIFETIME_HELPER_H_
#define MOJO_SHELL_PUBLIC_CPP_APP_LIFETIME_HELPER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"

namespace mojo {

class Shell;
class ShellConnection;
class AppLifetimeHelper;

// A service implementation should keep this object as a member variable to hold
// a reference on the application.
// Since services can live on different threads than the app, this class is
// safe to use on any thread. However, each instance should only be used on one
// thread at a time (otherwise there'll be races between the addref resulting
// from cloning and destruction).
class AppRefCount {
 public:
  ~AppRefCount();

  // When a service creates another object that is held by the client, it should
  // also vend to it a refcount using this method. That way if the caller stops
  // holding on to the service but keeps the reference to the object, the app is
  // still alive.
  scoped_ptr<AppRefCount> Clone();

 private:
  friend AppLifetimeHelper;

  AppRefCount(AppLifetimeHelper* app_lifetime_helper,
              scoped_refptr<base::SingleThreadTaskRunner> app_task_runner);

  // Don't need to use weak ptr because if the app thread is alive that means
  // app is.
  AppLifetimeHelper* app_lifetime_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> app_task_runner_;

#ifndef NDEBUG
  scoped_refptr<base::SingleThreadTaskRunner> clone_task_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AppRefCount);
};

// This is a helper class for apps to manage their lifetime, specifically so
// apps can quit when they're not used anymore.
// An app can contain an object of this class as a member variable. Each time it
// creates an instance of a service, it gives it a refcount using
// CreateAppRefCount. The service implementation then keeps that object as a
// member variable. When all the service implemenations go away, the app will be
// quit with a call to mojo::ShellConnection::Terminate().
class AppLifetimeHelper {
 public:
  explicit AppLifetimeHelper(Shell* shell);
  ~AppLifetimeHelper();

  scoped_ptr<AppRefCount> CreateAppRefCount();

 private:
  friend AppRefCount;
  void AddRef();
  void Release();

  friend ShellConnection;
  void OnQuit();

  Shell* shell_;
  int ref_count_;

  DISALLOW_COPY_AND_ASSIGN(AppLifetimeHelper);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APP_LIFETIME_HELPER_H_
