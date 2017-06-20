// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_INFO_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_INFO_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "content/common/content_export.h"

namespace service_manager {
class Service;
}

namespace content {

// ServiceInfo provides details necessary to construct and bind new instances
// of embedded services.
struct CONTENT_EXPORT ServiceInfo {
  using ServiceFactory =
      base::Callback<std::unique_ptr<service_manager::Service>()>;

  ServiceInfo();
  ServiceInfo(const ServiceInfo& other);
  ~ServiceInfo();

  // A factory function which will be called to produce a new Service
  // instance for this service whenever one is needed.
  ServiceFactory factory;

  // The task runner on which to construct and bind new Service instances
  // for this service. If null, behavior depends on the value of
  // |use_own_thread| below.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  // If |task_runner| is null, setting this to |true| will give each instance of
  // this service its own thread to run on. Setting this to |false| (the
  // default) will instead run the service on the main thread's task runner.
  //
  // If |task_runner| is not null, this value is ignored.
  bool use_own_thread = false;

  // If the service uses its own thread, this determines the type of the message
  // loop used by the thread.
  base::MessageLoop::Type message_loop_type = base::MessageLoop::TYPE_DEFAULT;

  // If the service uses its own thread, this determines the priority of the
  // thread.
  base::ThreadPriority thread_priority = base::ThreadPriority::NORMAL;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_INFO_H_
