// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_NOTE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_NOTE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// Note-specific options C++ code.
class OptionsNoteHandler : public ::options::OptionsPageUIHandler {
 public:
  OptionsNoteHandler();
  ~OptionsNoteHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;
  void RegisterMessages() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OptionsNoteHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_NOTE_HANDLER_H_
