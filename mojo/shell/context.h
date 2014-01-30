// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONTEXT_H_
#define MOJO_SHELL_CONTEXT_H_

#include "mojo/shell/loader.h"
#include "mojo/shell/service_connector.h"
#include "mojo/shell/storage.h"
#include "mojo/shell/task_runners.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // defined(OS_ANDROID)

namespace mojo {
namespace shell {

class DynamicServiceLoader;

class Context {
 public:
  Context();
  ~Context();

  TaskRunners* task_runners() { return &task_runners_; }
  Storage* storage() { return &storage_; }
  Loader* loader() { return &loader_; }
  ServiceConnector* service_connector() { return &service_connector_; }

#if defined(OS_ANDROID)
  jobject activity() const { return activity_.obj(); }
  void set_activity(jobject activity) { activity_.Reset(NULL, activity); }
#endif  // defined(OS_ANDROID)

 private:
  TaskRunners task_runners_;
  Storage storage_;
  Loader loader_;
  ServiceConnector service_connector_;
  scoped_ptr<DynamicServiceLoader> dynamic_service_loader_;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> activity_;
#endif  // defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONTEXT_H_
