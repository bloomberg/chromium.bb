// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "mojo/public/cpp/bindings/binding.h"

class InterventionsInternalsPageHandler
    : public mojom::InterventionsInternalsPageHandler,
      public MojoWebUIHandler {
 public:
  explicit InterventionsInternalsPageHandler(
      mojom::InterventionsInternalsPageHandlerRequest request);
  ~InterventionsInternalsPageHandler() override;

  // mojom::InterventionsInternalsPageHandler.
  void GetPreviewsEnabled(GetPreviewsEnabledCallback callback) override;

 private:
  mojo::Binding<mojom::InterventionsInternalsPageHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(InterventionsInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
