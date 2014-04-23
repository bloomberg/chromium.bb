// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_H_

#include "base/logging.h"
#include "ui/aura/window.h"

namespace mojo {
namespace services {
namespace view_manager {

class View {
 public:
  View(int32_t view_id);
  ~View();

  int32 id() const { return id_; }

  void Add(View* child);
  void Remove(View* child);

  View* GetParent();

 private:
  const int32_t id_;
  aura::Window window_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
