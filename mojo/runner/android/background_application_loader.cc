// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/android/background_application_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/shell/application_manager.h"

namespace mojo {
namespace runner {

BackgroundApplicationLoader::BackgroundApplicationLoader(
    scoped_ptr<ApplicationLoader> real_loader,
    const std::string& thread_name,
    base::MessageLoop::Type message_loop_type)
    : loader_(std::move(real_loader)),
      message_loop_type_(message_loop_type),
      thread_name_(thread_name),
      message_loop_created_(true, false) {}

BackgroundApplicationLoader::~BackgroundApplicationLoader() {
  if (thread_)
    thread_->Join();
}

void BackgroundApplicationLoader::Load(
    const GURL& url,
    InterfaceRequest<Application> application_request) {
  DCHECK(application_request.is_pending());
  if (!thread_) {
    // TODO(tim): It'd be nice if we could just have each Load call
    // result in a new thread like DynamicService{Loader, Runner}. But some
    // loaders are creating multiple ApplicationImpls (NetworkApplicationLoader)
    // sharing a delegate (etc). So we have to keep it single threaded, wait
    // for the thread to initialize, and post to the TaskRunner for subsequent
    // Load calls for now.
    thread_.reset(new base::DelegateSimpleThread(this, thread_name_));
    thread_->Start();
    message_loop_created_.Wait();
    DCHECK(task_runner_.get());
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundApplicationLoader::LoadOnBackgroundThread,
                 base::Unretained(this), url,
                 base::Passed(&application_request)));
}

void BackgroundApplicationLoader::Run() {
  base::MessageLoop message_loop(message_loop_type_);
  base::RunLoop loop;
  task_runner_ = message_loop.task_runner();
  quit_closure_ = loop.QuitClosure();
  message_loop_created_.Signal();
  loop.Run();

  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

void BackgroundApplicationLoader::LoadOnBackgroundThread(
    const GURL& url,
    InterfaceRequest<Application> application_request) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  loader_->Load(url, std::move(application_request));
}

}  // namespace runner
}  // namespace mojo
