// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_

#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// Implementation of the ProcessInternalsHandler interface, which is used to
// communicate between the chrome://process-internals/ WebUI and the browser
// process.
class ProcessInternalsHandlerImpl : public ::mojom::ProcessInternalsHandler {
 public:
  ProcessInternalsHandlerImpl(
      mojo::InterfaceRequest<::mojom::ProcessInternalsHandler> request);
  ~ProcessInternalsHandlerImpl() override;

  // mojom::ProcessInternalsHandler overrides:
  void GetIsolationMode(GetIsolationModeCallback callback) override;
  void GetIsolatedOrigins(GetIsolatedOriginsCallback callback) override;

 private:
  mojo::Binding<::mojom::ProcessInternalsHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInternalsHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
