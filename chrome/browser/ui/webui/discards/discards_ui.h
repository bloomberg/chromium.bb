// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"

// Controller for chrome://discards. Corresponding resources are in
// file://chrome/browser/resources/discards.
class DiscardsUI : public MojoWebUIController<mojom::DiscardsDetailsProvider> {
 public:
  explicit DiscardsUI(content::WebUI* web_ui);
  ~DiscardsUI() override;

 private:
  // MojoWebUIController overrides:
  void BindUIHandler(mojom::DiscardsDetailsProviderRequest request) override;

  std::unique_ptr<mojom::DiscardsDetailsProvider> ui_handler_;

  DISALLOW_COPY_AND_ASSIGN(DiscardsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
