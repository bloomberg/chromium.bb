// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_TASK_VIEWER_TASK_VIEWER_APPLICATION_DELEGATE_H_
#define MASH_TASK_VIEWER_TASK_VIEWER_APPLICATION_DELEGATE_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace mojo {
class ApplicationConnection;
}

namespace views {
class AuraInit;
}

namespace mash {
namespace task_viewer {

class TaskViewerApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  TaskViewerApplicationDelegate();
  ~TaskViewerApplicationDelegate() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;

  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(TaskViewerApplicationDelegate);
};

}  // namespace task_viewer
}  // namespace mash

#endif  // MASH_TASK_VIEWER_TASK_VIEWER_APPLICATION_DELEGATE_H_
