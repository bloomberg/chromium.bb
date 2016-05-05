// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_
#define CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "services/shell/public/cpp/shell_client.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

struct CONTENT_EXPORT MojoApplicationInfo {
  using ApplicationFactory = base::Callback<std::unique_ptr<shell::ShellClient>(
      const base::Closure& quit_closure)>;

  MojoApplicationInfo();
  MojoApplicationInfo(const MojoApplicationInfo& other);
  ~MojoApplicationInfo();

  ApplicationFactory application_factory;
  scoped_refptr<base::SingleThreadTaskRunner> application_task_runner;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_
