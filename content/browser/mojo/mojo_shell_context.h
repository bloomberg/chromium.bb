// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_

#include <map>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/shell.h"

namespace catalog {
class Factory;
}

namespace mojo {
class ShellClient;
}

namespace content {

// MojoShellContext hosts the browser's ApplicationManager, coordinating
// app registration and interconnection.
class CONTENT_EXPORT MojoShellContext {
 public:
  using StaticApplicationMap =
      std::map<std::string, base::Callback<scoped_ptr<mojo::ShellClient>()>>;

  MojoShellContext(scoped_refptr<base::SingleThreadTaskRunner> file_thread,
                   scoped_refptr<base::SingleThreadTaskRunner> db_thread);
  ~MojoShellContext();

  // Connects an application at |name| and gets a handle to its exposed
  // services. This is only intended for use in browser code that's not part of
  // some Mojo application. May be called from any thread. |requestor_name| is
  // given to the target application as the requestor's name upon connection.
  static void ConnectToApplication(
      const std::string& user_id,
      const std::string& name,
      const std::string& requestor_name,
      mojo::shell::mojom::InterfaceProviderRequest request,
      mojo::shell::mojom::InterfaceProviderPtr exposed_services,
      const mojo::shell::mojom::Connector::ConnectCallback& callback);

  static void SetApplicationsForTest(const StaticApplicationMap* apps);

 private:
  class BuiltinManifestProvider;
  class Proxy;
  friend class Proxy;

  void ConnectToApplicationOnOwnThread(
      const std::string& user_id,
      const std::string& name,
      const std::string& requestor_name,
      mojo::shell::mojom::InterfaceProviderRequest request,
      mojo::shell::mojom::InterfaceProviderPtr exposed_services,
      const mojo::shell::mojom::Connector::ConnectCallback& callback);

  static base::LazyInstance<scoped_ptr<Proxy>> proxy_;

  scoped_ptr<BuiltinManifestProvider> manifest_provider_;
  scoped_ptr<catalog::Factory> catalog_;
  scoped_ptr<mojo::shell::Shell> shell_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
