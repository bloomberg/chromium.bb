// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace input_method {
class InputMethodEngine : public InputMethodEngineBase {
 public:
  InputMethodEngine();

  ~InputMethodEngine() override;

  // ui::IMEEngineHandlerInterface:
  bool SendKeyEvents(int context_id,
                     const std::vector<KeyboardEvent>& events) override;
  bool IsActive() const override;
  std::string GetExtensionId() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngine);
};

}  // namespace input_method

#endif  // CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
