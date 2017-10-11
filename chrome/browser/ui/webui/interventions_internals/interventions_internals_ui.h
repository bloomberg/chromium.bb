// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"
#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"
#include "components/previews/core/previews_logger.h"

// The WebUI for chrome://interventions-internals.
class InterventionsInternalsUI
    : public MojoWebUIController<mojom::InterventionsInternalsPageHandler> {
 public:
  explicit InterventionsInternalsUI(content::WebUI* web_ui);
  ~InterventionsInternalsUI() override;

 private:
  // MojoWebUIController overrides:
  void BindUIHandler(
      mojom::InterventionsInternalsPageHandlerRequest request) override;

  // The PreviewsLogger that this handler is listening to.
  previews::PreviewsLogger* logger_;

  std::unique_ptr<InterventionsInternalsPageHandler> page_handler_;

  DISALLOW_COPY_AND_ASSIGN(InterventionsInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_
