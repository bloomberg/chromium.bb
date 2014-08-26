// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_BACKGROUND_SHELL_APPLICATION_LOADER_H_
#define MOJO_APPLICATION_MANAGER_BACKGROUND_SHELL_APPLICATION_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/application_manager/application_loader.h"

namespace mojo {

// TODO(tim): Eventually this should be Android-only to support services
// that we need to bundle with the shell (such as NetworkService). Perhaps
// we should move it to shell/ as well.
class MOJO_APPLICATION_MANAGER_EXPORT BackgroundShellApplicationLoader
    : public ApplicationLoader,
      public base::DelegateSimpleThread::Delegate {
 public:
  BackgroundShellApplicationLoader(scoped_ptr<ApplicationLoader> real_loader,
                                   const std::string& thread_name,
                                   base::MessageLoop::Type message_loop_type);
  virtual ~BackgroundShellApplicationLoader();

  // ApplicationLoader overrides:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnApplicationError(ApplicationManager* manager,
                                  const GURL& url) OVERRIDE;

 private:
  class BackgroundLoader;

  // |base::DelegateSimpleThread::Delegate| method:
  virtual void Run() OVERRIDE;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ApplicationManager
  // isn't thread safe).
  void LoadOnBackgroundThread(ApplicationManager* manager,
                              const GURL& url,
                              ScopedMessagePipeHandle* shell_handle);
  void OnApplicationErrorOnBackgroundThread(ApplicationManager* manager,
                                            const GURL& url);
  bool quit_on_shutdown_;
  scoped_ptr<ApplicationLoader> loader_;

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

  DISALLOW_COPY_AND_ASSIGN(BackgroundShellApplicationLoader);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_BACKGROUND_SHELL_APPLICATION_LOADER_H_
