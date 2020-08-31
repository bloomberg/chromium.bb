// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom-forward.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace previews {
class PreviewsUIService;
}  // namespace previews

// The WebUI for chrome://interventions-internals.
class InterventionsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit InterventionsInternalsUI(content::WebUI* web_ui);
  ~InterventionsInternalsUI() override;

  // Instantiates implementor of the mojom::InterventionsInternalsPageHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::InterventionsInternalsPageHandler> receiver);

 private:
  // The PreviewsUIService associated with this UI.
  previews::PreviewsUIService* previews_ui_service_;

  std::unique_ptr<InterventionsInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(InterventionsInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_UI_H_
