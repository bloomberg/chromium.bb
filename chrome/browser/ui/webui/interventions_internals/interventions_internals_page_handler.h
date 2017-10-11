// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_logger_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

class InterventionsInternalsPageHandler
    : public previews::PreviewsLoggerObserver,
      public mojom::InterventionsInternalsPageHandler,
      public MojoWebUIHandler {
 public:
  InterventionsInternalsPageHandler(
      mojom::InterventionsInternalsPageHandlerRequest request,
      previews::PreviewsLogger* logger);
  ~InterventionsInternalsPageHandler() override;

  // mojom::InterventionsInternalsPageHandler:
  void GetPreviewsEnabled(GetPreviewsEnabledCallback callback) override;
  void SetClientPage(mojom::InterventionsInternalsPagePtr page) override;

  // previews::PreviewsLoggerObserver:
  void OnNewMessageLogAdded(
      const previews::PreviewsLogger::MessageLog& message) override;

 private:
  mojo::Binding<mojom::InterventionsInternalsPageHandler> binding_;

  // The PreviewsLogger that this handler is listening to, and guaranteed to
  // outlive |this|.
  previews::PreviewsLogger* logger_;

  // Handle back to the page by which we can pass in new log messages.
  mojom::InterventionsInternalsPagePtr page_;

  DISALLOW_COPY_AND_ASSIGN(InterventionsInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERVENTIONS_INTERNALS_INTERVENTIONS_INTERNALS_PAGE_HANDLER_H_
