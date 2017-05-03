// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_TASK_VIEWER_TASK_VIEWER_H_
#define MASH_TASK_VIEWER_TASK_VIEWER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
}

namespace mash {
namespace task_viewer {

class TaskViewer : public service_manager::Service,
                   public ::mash::mojom::Launchable {
 public:
  TaskViewer();
  ~TaskViewer() override;

  void RemoveWindow(views::Widget* widget);

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ::mash::mojom::Launchable:
  void Launch(uint32_t what, ::mash::mojom::LaunchMode how) override;

  void Create(const service_manager::BindSourceInfo& source_info,
              ::mash::mojom::LaunchableRequest request);

  mojo::BindingSet<::mash::mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(TaskViewer);
};

}  // namespace task_viewer
}  // namespace mash

#endif  // MASH_TASK_VIEWER_TASK_VIEWER_H_
