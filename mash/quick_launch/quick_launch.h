// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_QUICK_LAUNCH_QUICK_LAUNCH_H_
#define MASH_QUICK_LAUNCH_QUICK_LAUNCH_H_

#include <memory>

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
namespace quick_launch {

class QuickLaunch : public service_manager::Service,
                    public ::mash::mojom::Launchable {
 public:
  QuickLaunch();
  ~QuickLaunch() override;

  void RemoveWindow(views::Widget* window);

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ::mash::mojom::Launchable:
  void Launch(uint32_t what, ::mash::mojom::LaunchMode how) override;

  void Create(::mash::mojom::LaunchableRequest request);

  mojo::BindingSet<::mash::mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunch);
};

}  // namespace quick_launch
}  // namespace mash

#endif  // MASH_QUICK_LAUNCH_QUICK_LAUNCH_H_
