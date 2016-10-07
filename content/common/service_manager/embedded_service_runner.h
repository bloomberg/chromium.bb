// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_
#define CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "content/public/common/service_info.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace content {

// Hosts an in-process service instance that supports multiple Service
// connections. The first incoming connection will invoke a provided factory
// function to instantiate the service, and the service will automatically be
// torn down when its last connection is lost. The service may be launched and
// torn down multiple times by a single EmbeddedServiceRunner instance.
class EmbeddedServiceRunner {
 public:
  // Constructs a runner which hosts a service. If an existing instance of the
  // service is not running when an incoming connection is made, details from
  // |info| will be used to construct a new instance.
  EmbeddedServiceRunner(const base::StringPiece& name,
                        const ServiceInfo& info);
  ~EmbeddedServiceRunner();

  // Binds an incoming ServiceRequest for this service. If the service isn't
  // already running, it is started. Otherwise the request is bound to the
  // running instance.
  void BindServiceRequest(shell::mojom::ServiceRequest request);

  // Sets a callback to run after the service loses its last connection and is
  // torn down.
  void SetQuitClosure(const base::Closure& quit_closure);

 private:
  class Instance;

  void OnQuit();

  // A reference to the service instance which may operate on the
  // |task_runner_|'s thread.
  scoped_refptr<Instance> instance_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<EmbeddedServiceRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedServiceRunner);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_
