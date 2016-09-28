// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include "base/id_map.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"

namespace content {

class EmbeddedWorkerInstanceClientImpl
    : public mojom::EmbeddedWorkerInstanceClient {
 public:
  static void Create(
      EmbeddedWorkerDispatcher* dispatcher,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerInstanceClient> request);

  explicit EmbeddedWorkerInstanceClientImpl(
      EmbeddedWorkerDispatcher* dispatcher);
  ~EmbeddedWorkerInstanceClientImpl() override;

  // Implementation of mojo interface
  void StartWorker(const EmbeddedWorkerStartParams& params) override;

 private:
  EmbeddedWorkerDispatcher* dispatcher_;

  EmbeddedWorkerDispatcher::WorkerWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
