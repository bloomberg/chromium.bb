// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/service_manager/background_shell_service_loader.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/service_manager/service_manager.h"

namespace mojo {

class BackgroundShellServiceLoader::BackgroundLoader {
 public:
  explicit BackgroundLoader(ServiceLoader* loader) : loader_(loader) {}
  ~BackgroundLoader() {}

  void LoadService(ServiceManager* manager,
                   const GURL& url,
                   ScopedMessagePipeHandle shell_handle) {
    loader_->LoadService(manager, url, shell_handle.Pass());
  }

  void OnServiceError(ServiceManager* manager, const GURL& url) {
    loader_->OnServiceError(manager, url);
  }

 private:
  ServiceLoader* loader_;  // Owned by BackgroundShellServiceLoader

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoader);
};

BackgroundShellServiceLoader::BackgroundShellServiceLoader(
    scoped_ptr<ServiceLoader> real_loader,
    const std::string& thread_name,
    base::MessageLoop::Type message_loop_type)
    : loader_(real_loader.Pass()),
      message_loop_type_(message_loop_type),
      thread_name_(thread_name),
      message_loop_created_(true, false),
      background_loader_(NULL) {
}

BackgroundShellServiceLoader::~BackgroundShellServiceLoader() {
  if (thread_)
    thread_->Join();
}

void BackgroundShellServiceLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle shell_handle) {
  if (!thread_) {
    // TODO(tim): It'd be nice if we could just have each LoadService call
    // result in a new thread like DynamicService{Loader, Runner}. But some
    // loaders are creating multiple ApplicationImpls (NetworkServiceLoader)
    // sharing a delegate (etc). So we have to keep it single threaded, wait
    // for the thread to initialize, and post to the TaskRunner for subsequent
    // LoadService calls for now.
    thread_.reset(new base::DelegateSimpleThread(this, thread_name_));
    thread_->Start();
    message_loop_created_.Wait();
    DCHECK(task_runner_);
  }

  task_runner_->PostTask(FROM_HERE,
      base::Bind(&BackgroundShellServiceLoader::LoadServiceOnBackgroundThread,
                 base::Unretained(this), manager, url,
                 base::Owned(
                    new ScopedMessagePipeHandle(shell_handle.Pass()))));
}

void BackgroundShellServiceLoader::OnServiceError(
    ServiceManager* manager, const GURL& url) {
  task_runner_->PostTask(FROM_HERE,
      base::Bind(
          &BackgroundShellServiceLoader::OnServiceErrorOnBackgroundThread,
          base::Unretained(this), manager, url));
}

void BackgroundShellServiceLoader::Run() {
  base::MessageLoop message_loop(message_loop_type_);
  base::RunLoop loop;
  task_runner_ = message_loop.task_runner();
  quit_closure_ = loop.QuitClosure();
  message_loop_created_.Signal();
  loop.Run();

  delete background_loader_;
  background_loader_ = NULL;
  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

void BackgroundShellServiceLoader::LoadServiceOnBackgroundThread(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle* shell_handle) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (!background_loader_)
    background_loader_ = new BackgroundLoader(loader_.get());
  background_loader_->LoadService(manager, url, shell_handle->Pass());
}

void BackgroundShellServiceLoader::OnServiceErrorOnBackgroundThread(
    ServiceManager* manager, const GURL& url) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (!background_loader_)
    background_loader_ = new BackgroundLoader(loader_.get());
  background_loader_->OnServiceError(manager, url);
}

}  // namespace mojo
