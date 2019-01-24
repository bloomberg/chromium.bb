// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "fuchsia/runners/common/web_content_runner.h"

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public WebContentRunner {
 public:
  CastRunner(base::fuchsia::ServiceDirectory* service_directory,
             chromium::web::ContextPtr context,
             chromium::cast::ApplicationConfigManagerPtr app_config_manager,
             base::OnceClosure on_idle_closure);

  ~CastRunner() override;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

 private:
  chromium::cast::ApplicationConfigManagerPtr app_config_manager_;

  void GetConfigCallback(
      fuchsia::sys::StartupInfo startup_info,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request,
      chromium::cast::ApplicationConfigPtr app_config);

  DISALLOW_COPY_AND_ASSIGN(CastRunner);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
