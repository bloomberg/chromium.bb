// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_INTERNALS_UI_H_

#include "base/macros.h"
#include "chrome/browser/autofill/autofill_internals_logging_impl.h"
#include "components/autofill/core/browser/autofill_internals_logging.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

class AutofillInternalsUI : public content::WebUIController,
                            public content::WebContentsObserver {
 public:
  explicit AutofillInternalsUI(content::WebUI* web_ui);

  // WebContentsObserver implementation.
  void DidStartLoading() override;
  void DidStopLoading() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_INTERNALS_UI_H_
