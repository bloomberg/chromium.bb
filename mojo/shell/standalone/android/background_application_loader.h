// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_ANDROID_BACKGROUND_APPLICATION_LOADER_H_
#define MOJO_SHELL_STANDALONE_ANDROID_BACKGROUND_APPLICATION_LOADER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "mojo/shell/application_loader.h"

namespace mojo {
namespace shell {

class BackgroundApplicationLoader
    : public ApplicationLoader,
      public base::DelegateSimpleThread::Delegate {
 public:
  BackgroundApplicationLoader(scoped_ptr<ApplicationLoader> real_loader,
                              const std::string& thread_name,
                              base::MessageLoop::Type message_loop_type);
  ~BackgroundApplicationLoader() override;

  // ApplicationLoader overrides:
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override;

 private:
  // |base::DelegateSimpleThread::Delegate| method:
  void Run() override;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  void LoadOnBackgroundThread(const GURL& url,
                              InterfaceRequest<mojom::ShellClient> request);
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

  DISALLOW_COPY_AND_ASSIGN(BackgroundApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_ANDROID_BACKGROUND_APPLICATION_LOADER_H_
