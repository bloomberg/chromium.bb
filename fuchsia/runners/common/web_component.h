// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_
#define FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/app/cpp/fidl.h>
#include <fuchsia/ui/viewsv1/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "url/gurl.h"

class WebContentRunner;

// Base component implementation for web-based content Runners. Each instance
// manages the lifetime of its own chromium::web::Frame, including associated
// resources and service bindings.  Runners for specialized web-based content
// (e.g. Cast applications) can extend this class to configure the Frame to
// their needs, publish additional APIs, etc.
// TODO(crbug.com/899348): Remove fuchsia::ui::viewsv1::ViewProvider.
class WebComponent : public fuchsia::sys::ComponentController,
                     public fuchsia::ui::app::ViewProvider,
                     public fuchsia::ui::viewsv1::ViewProvider {
 public:
  ~WebComponent() override;

  // Creates a WebComponent and navigates its Frame to |url|.
  static std::unique_ptr<WebComponent> ForUrlRequest(
      WebContentRunner* runner,
      const GURL& url,
      fuchsia::sys::StartupInfo startup_info,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request);

  chromium::web::Frame* frame() { return frame_.get(); }

 protected:
  // Creates a WebComponent encapsulating a web.Frame. A ViewProvider service
  // will be published to the service-directory specified in |startup_info|, and
  // if |controller_request| is valid then it will be bound to this component,
  // and the component configured to teardown if that channel closes.
  // |runner| must outlive this component.
  WebComponent(WebContentRunner* runner,
               fuchsia::sys::StartupInfo startup_info,
               fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                   controller_request);

  // fuchsia::sys::ComponentController implementation.
  void Kill() override;
  void Detach() override;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateView(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;

  // fuchsia::ui::viewsv1::ViewProvider implementation.
  void CreateView(
      fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) override;

  // Reports the supplied exit-code and reason to the |controller_binding_| and
  // requests that the |runner_| delete this component.
  void DestroyComponent(int termination_exit_code,
                        fuchsia::sys::TerminationReason reason);

  base::fuchsia::ServiceDirectory* service_directory() {
    return service_directory_.get();
  }

 private:
  WebContentRunner* runner_ = nullptr;

  chromium::web::FramePtr frame_;

  fidl::Binding<fuchsia::sys::ComponentController> controller_binding_;

  // Objects used for binding and exporting the ViewProvider service.
  std::unique_ptr<base::fuchsia::ServiceDirectory> service_directory_;
  std::unique_ptr<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>
      view_provider_binding_;
  std::unique_ptr<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::viewsv1::ViewProvider>>
      legacy_view_provider_binding_;

  // Termination reason and exit-code to be reported via the
  // sys::ComponentController::OnTerminated event.
  fuchsia::sys::TerminationReason termination_reason_ =
      fuchsia::sys::TerminationReason::UNKNOWN;
  int termination_exit_code_ = 0;

  bool view_is_bound_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebComponent);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_
