// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/app_lifetime_helper.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {

AppRefCount::AppRefCount(
    AppLifetimeHelper* app_lifetime_helper,
    scoped_refptr<base::SingleThreadTaskRunner> app_task_runner)
    : app_lifetime_helper_(app_lifetime_helper),
      app_task_runner_(app_task_runner) {
}

AppRefCount::~AppRefCount() {
#ifndef NDEBUG
  // Ensure that this object is used on only one thread at a time, or else there
  // could be races where the object is being reset on one thread and cloned on
  // another.
  if (clone_task_runner_) {
    DCHECK(clone_task_runner_->BelongsToCurrentThread());
  }
#endif

  if (app_task_runner_->BelongsToCurrentThread()) {
    app_lifetime_helper_->Release();
    return;
  }

  app_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AppLifetimeHelper::Release,
                 base::Unretained(app_lifetime_helper_)));
}

scoped_ptr<AppRefCount> AppRefCount::Clone() {
  if (app_task_runner_->BelongsToCurrentThread()) {
    app_lifetime_helper_->AddRef();
  } else {
    app_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&AppLifetimeHelper::AddRef,
                   base::Unretained(app_lifetime_helper_)));
  }

#ifndef NDEBUG
  // Ensure that this object is used on only one thread at a time, or else there
  // could be races where the object is being reset on one thread and cloned on
  // another.
  if (clone_task_runner_) {
    DCHECK(clone_task_runner_->BelongsToCurrentThread());
  } else {
    clone_task_runner_ = base::MessageLoop::current()->task_runner();
  }
#endif

  return make_scoped_ptr(new AppRefCount(
      app_lifetime_helper_, app_task_runner_));
}

AppLifetimeHelper::AppLifetimeHelper(Shell* shell)
    : shell_(shell), ref_count_(0) {
}

AppLifetimeHelper::~AppLifetimeHelper() {
}

scoped_ptr<AppRefCount> AppLifetimeHelper::CreateAppRefCount() {
  AddRef();
  return make_scoped_ptr(new AppRefCount(
      this, base::MessageLoop::current()->task_runner()));
}

void AppLifetimeHelper::AddRef() {
  ref_count_++;
}

void AppLifetimeHelper::Release() {
  if (!--ref_count_) {
    if (shell_)
      shell_->Quit();
  }
}

void AppLifetimeHelper::OnQuit() {
  shell_ = nullptr;
}

}  // namespace mojo
