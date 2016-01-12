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
#include "content/common/content_export.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

class GURL;

namespace mojo {
class ApplicationDelegate;
}

namespace content {

// MojoShellContext hosts the browser's ApplicationManager, coordinating
// app registration and interconnection.
class CONTENT_EXPORT MojoShellContext {
 public:
  using StaticApplicationMap =
      std::map<GURL, base::Callback<scoped_ptr<mojo::ApplicationDelegate>()>>;

  MojoShellContext();
  ~MojoShellContext();

  // Connects an application at |url| and gets a handle to its exposed services.
  // This is only intended for use in browser code that's not part of some Mojo
  // application. May be called from any thread. |requestor_url| is given to
  // the target application as the requestor's URL upon connection.
  static void ConnectToApplication(
      const GURL& url,
      const GURL& requestor_url,
      mojo::InterfaceRequest<mojo::ServiceProvider> request,
      mojo::ServiceProviderPtr exposed_services,
      const mojo::shell::CapabilityFilter& filter,
      const mojo::Shell::ConnectToApplicationCallback& callback);

  static void SetApplicationsForTest(const StaticApplicationMap* apps);

 private:
  class Proxy;
  friend class Proxy;

  void ConnectToApplicationOnOwnThread(
      const GURL& url,
      const GURL& requestor_url,
      mojo::InterfaceRequest<mojo::ServiceProvider> request,
      mojo::ServiceProviderPtr exposed_services,
      const mojo::shell::CapabilityFilter& filter,
      const mojo::Shell::ConnectToApplicationCallback& callback);

  static base::LazyInstance<scoped_ptr<Proxy>> proxy_;

  scoped_ptr<mojo::shell::ApplicationManager> application_manager_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
