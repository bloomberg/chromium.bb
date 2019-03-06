// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"

namespace content {

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context) {
  return base::WrapRefCounted(new EmbeddedWorkerRegistry(context, 0));
}

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    EmbeddedWorkerRegistry* old_registry) {
  scoped_refptr<EmbeddedWorkerRegistry> registry =
      new EmbeddedWorkerRegistry(
          context,
          old_registry->next_embedded_worker_id_);
  return registry;
}

std::unique_ptr<EmbeddedWorkerInstance> EmbeddedWorkerRegistry::CreateWorker(
    ServiceWorkerVersion* owner_version) {
  std::unique_ptr<EmbeddedWorkerInstance> worker(new EmbeddedWorkerInstance(
      context_, owner_version, next_embedded_worker_id_));
  worker_map_[next_embedded_worker_id_++] = worker.get();
  return worker;
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorker(
    int embedded_worker_id) {
  auto found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end())
    return nullptr;
  return found->second;
}

EmbeddedWorkerRegistry::EmbeddedWorkerRegistry(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    int initial_embedded_worker_id)
    : context_(context),
      next_embedded_worker_id_(initial_embedded_worker_id),
      initial_embedded_worker_id_(initial_embedded_worker_id) {
}

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() = default;

void EmbeddedWorkerRegistry::RemoveWorker(int embedded_worker_id) {
  DCHECK(base::ContainsKey(worker_map_, embedded_worker_id));
  worker_map_.erase(embedded_worker_id);
}

}  // namespace content
