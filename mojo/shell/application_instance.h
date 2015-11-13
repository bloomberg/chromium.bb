// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_INSTANCE_H_
#define MOJO_SHELL_APPLICATION_INSTANCE_H_

#include <set>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;

// Encapsulates a connection to an instance of an application, tracked by the
// shell's ApplicationManager.
class ApplicationInstance : public Shell {
 public:
  // |requesting_content_handler_id| is the id of the content handler that
  // loaded this app. If the app was not loaded by a content handler the id
  // is kInvalidContentHandlerID.
  ApplicationInstance(ApplicationPtr application,
                      ApplicationManager* manager,
                      const Identity& identity,
                      uint32_t requesting_content_handler_id,
                      const base::Closure& on_application_end);

  ~ApplicationInstance() override;

  void InitializeApplication();

  void ConnectToClient(scoped_ptr<ConnectToApplicationParams> params);

  Application* application() { return application_.get(); }
  const Identity& identity() const { return identity_; }
  base::Closure on_application_end() const { return on_application_end_; }
  void set_requesting_content_handler_id(uint32_t id) {
    requesting_content_handler_id_ = id;
  }
  uint32_t requesting_content_handler_id() const {
    return requesting_content_handler_id_;
  }

 private:
  // Shell implementation:
  void ConnectToApplication(
      URLRequestPtr app_request,
      InterfaceRequest<ServiceProvider> services,
      ServiceProviderPtr exposed_services,
      CapabilityFilterPtr filter,
      const ConnectToApplicationCallback& callback) override;
  void QuitApplication() override;

  void CallAcceptConnection(scoped_ptr<ConnectToApplicationParams> params);

  void OnConnectionError();

  void OnQuitRequestedResult(bool can_quit);

  ApplicationManager* const manager_;
  const Identity identity_;
  const bool allow_any_application_;
  uint32_t requesting_content_handler_id_;
  base::Closure on_application_end_;
  ApplicationPtr application_;
  Binding<Shell> binding_;
  bool queue_requests_;
  std::vector<ConnectToApplicationParams*> queued_client_requests_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationInstance);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_INSTANCE_H_
