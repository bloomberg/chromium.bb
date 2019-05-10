// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace machine_learning {

MachineLearningInternalsUI::MachineLearningInternalsUI(
    content::WebUI* const web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* const source = content::WebUIDataSource::Create(
      chrome::kChromeUIMachineLearningInternalsHost);

  source->SetDefaultResource(IDR_MACHINE_LEARNING_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
  AddHandlerToRegistry(base::BindRepeating(
      &MachineLearningInternalsUI::BindMachineLearningInternalsPageHandler,
      base::Unretained(this)));
}

MachineLearningInternalsUI::~MachineLearningInternalsUI() = default;

void MachineLearningInternalsUI::BindMachineLearningInternalsPageHandler(
    mojom::PageHandlerRequest request) {
  page_handler_.reset(
      new MachineLearningInternalsPageHandler(std::move(request)));
}

}  // namespace machine_learning
}  // namespace chromeos
