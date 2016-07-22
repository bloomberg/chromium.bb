// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "services/shell/service_manager.h"

namespace catalog {
class Catalog;
}

namespace content {

// MojoShellContext hosts the browser's ApplicationManager, coordinating
// app registration and interconnection.
class CONTENT_EXPORT MojoShellContext {
 public:
  MojoShellContext();
  ~MojoShellContext();

  // Returns a shell::Connector that can be used on the IO thread.
  static shell::Connector* GetConnectorForIOThread();

 private:
  class BuiltinManifestProvider;

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<catalog::Catalog> catalog_;
  std::unique_ptr<shell::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
