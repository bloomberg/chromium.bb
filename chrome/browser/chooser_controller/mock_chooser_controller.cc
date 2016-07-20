// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/mock_chooser_controller.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

MockChooserController::MockChooserController(content::RenderFrameHost* owner)
    : ChooserController(owner,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME) {}

MockChooserController::~MockChooserController() {}

base::string16 MockChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 MockChooserController::GetOkButtonLabel() const {
  return base::string16();
}

size_t MockChooserController::NumOptions() const {
  return option_names_.size();
}

base::string16 MockChooserController::GetOption(size_t index) const {
  return option_names_[index];
}

void MockChooserController::RefreshOptions() {}

base::string16 MockChooserController::GetStatus() const {
  return base::string16();
}

void MockChooserController::OptionAdded(const base::string16 option_name) {
  option_names_.push_back(option_name);
  if (observer())
    observer()->OnOptionAdded(option_names_.size() - 1);
}

void MockChooserController::OptionRemoved(const base::string16 option_name) {
  for (auto it = option_names_.begin(); it != option_names_.end(); ++it) {
    if (*it == option_name) {
      size_t index = it - option_names_.begin();
      option_names_.erase(it);
      if (observer())
        observer()->OnOptionRemoved(index);
      return;
    }
  }
}
