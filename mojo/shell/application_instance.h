// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_INSTANCE_H_
#define MOJO_SHELL_APPLICATION_INSTANCE_H_

#include "base/callback.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/identity.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;

// Encapsulates a connection to an instance of an application, tracked by the
// shell's ApplicationManager.
// TODO(beng): Currently this provides a default implementation of the Shell
//             interface. This should be moved into a separate class RootShell
//             which is instantiated when no other Shell implementation is
//             provided via ConnectToApplication().
class ApplicationInstance : public Shell {
 public:
  ApplicationInstance(ApplicationPtr application,
                      ApplicationManager* manager,
                      const Identity& resolved_identity,
                      const base::Closure& on_application_end);

  ~ApplicationInstance() override;

  void InitializeApplication();

  void ConnectToClient(const GURL& requested_url,
                       const GURL& requestor_url,
                       InterfaceRequest<ServiceProvider> services,
                       ServiceProviderPtr exposed_services);

  Application* application() { return application_.get(); }
  const Identity& identity() const { return identity_; }
  base::Closure on_application_end() const { return on_application_end_; }

 private:
  // Shell implementation:
  void ConnectToApplication(mojo::URLRequestPtr app_request,
                            InterfaceRequest<ServiceProvider> services,
                            ServiceProviderPtr exposed_services) override;
  void QuitApplication() override;

  void OnConnectionError();

  void OnQuitRequestedResult(bool can_quit);

  struct QueuedClientRequest {
    QueuedClientRequest();
    ~QueuedClientRequest();
    GURL requested_url;
    GURL requestor_url;
    InterfaceRequest<ServiceProvider> services;
    ServiceProviderPtr exposed_services;
  };

  ApplicationManager* const manager_;
  const Identity identity_;
  base::Closure on_application_end_;
  ApplicationPtr application_;
  Binding<Shell> binding_;
  bool queue_requests_;
  std::vector<QueuedClientRequest*> queued_client_requests_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationInstance);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_INSTANCE_H_
