// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/embedded_ui_service_info_factory.h"

#include "content/public/browser/discardable_shared_memory_manager.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

namespace {

std::unique_ptr<service_manager::Service> CreateEmbeddedUIService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr,
    discardable_memory::DiscardableSharedMemoryManager* memory_manager) {
  ui::Service::InProcessConfig config;
  config.resource_runner = task_runner;
  config.image_cursors_set_weak_ptr = image_cursors_set_weak_ptr;
  config.memory_manager = memory_manager;
  return base::MakeUnique<ui::Service>(&config);
}

}  // namespace

service_manager::EmbeddedServiceInfo CreateEmbeddedUIServiceInfo(
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr) {
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::Bind(
      &CreateEmbeddedUIService, base::ThreadTaskRunnerHandle::Get(),
      image_cursors_set_weak_ptr, content::GetDiscardableSharedMemoryManager());
  info.use_own_thread = true;
  info.message_loop_type = base::MessageLoop::TYPE_UI;
  info.thread_priority = base::ThreadPriority::DISPLAY;
  return info;
}
