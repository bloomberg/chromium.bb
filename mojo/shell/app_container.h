// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CONTAINER_H_
#define MOJO_SHELL_APP_CONTAINER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/shell/loader.h"

namespace base {
class FilePath;
class PlatformThreadHandle;
}

namespace mojo {
namespace services {
class NativeViewportImpl;
}
namespace shell {

class Context;

// A container class that runs an app on its own thread.
class AppContainer
    : public Loader::Delegate,
      public base::DelegateSimpleThread::Delegate {
 public:
  explicit AppContainer(Context* context);
  virtual ~AppContainer();

  void Load(const GURL& app_url);

 private:
  // From Loader::Delegate.
  virtual void DidCompleteLoad(const GURL& app_url,
                               const base::FilePath& app_path) OVERRIDE;

  // From base::DelegateSimpleThread::Delegate.
  virtual void Run() OVERRIDE;

  void AppCompleted();

  Context* context_;
  base::FilePath app_path_;
  ScopedMessagePipeHandle shell_handle_;
  ScopedMessagePipeHandle app_handle_;
  base::Closure ack_closure_;
  scoped_ptr<Loader::Job> request_;
  scoped_ptr<base::DelegateSimpleThread> thread_;
  scoped_ptr<services::NativeViewportImpl> native_viewport_;

  base::WeakPtrFactory<AppContainer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppContainer);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CONTAINER_H_
