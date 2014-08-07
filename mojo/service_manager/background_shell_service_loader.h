// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_BACKGROUND_SHELL_SERVICE_LOADER_H_
#define MOJO_SERVICE_MANAGER_BACKGROUND_SHELL_SERVICE_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

// TODO(tim): Eventually this should be Android-only to support services
// that we need to bundle with the shell (such as NetworkService). Perhaps
// we should move it to shell/ as well.
class MOJO_SERVICE_MANAGER_EXPORT BackgroundShellServiceLoader :
    public ServiceLoader,
    public base::DelegateSimpleThread::Delegate {
 public:
  BackgroundShellServiceLoader(scoped_ptr<ServiceLoader> real_loader,
                               const std::string& thread_name,
                               base::MessageLoop::Type message_loop_type);
  virtual ~BackgroundShellServiceLoader();

  // ServiceLoader overrides:
  virtual void Load(ServiceManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

 private:
  class BackgroundLoader;

  // |base::DelegateSimpleThread::Delegate| method:
  virtual void Run() OVERRIDE;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ServiceManager isn't
  // thread safe).
  void LoadOnBackgroundThread(ServiceManager* manager,
                              const GURL& url,
                              ScopedMessagePipeHandle* shell_handle);
  void OnServiceErrorOnBackgroundThread(ServiceManager* manager,
                                        const GURL& url);
  bool quit_on_shutdown_;
  scoped_ptr<ServiceLoader> loader_;

  const base::MessageLoop::Type message_loop_type_;
  const std::string thread_name_;

  // Created on |thread_| during construction of |this|. Protected against
  // uninitialized use by |message_loop_created_|, and protected against
  // use-after-free by holding a reference to the thread-safe object. Note
  // that holding a reference won't hold |thread_| from exiting.
  scoped_refptr<base::TaskRunner> task_runner_;
  base::WaitableEvent message_loop_created_;

  // Lives on |thread_|.
  base::Closure quit_closure_;

  scoped_ptr<base::DelegateSimpleThread> thread_;

  // Lives on |thread_|. Trivial interface that calls through to |loader_|.
  BackgroundLoader* background_loader_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundShellServiceLoader);
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_BACKGROUND_SHELL_SERVICE_LOADER_H_
