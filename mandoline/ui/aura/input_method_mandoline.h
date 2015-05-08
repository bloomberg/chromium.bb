// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#ifndef MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_
#define MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_

namespace mandoline {

class InputMethodMandoline : public ui::InputMethodBase {
 public:
  explicit InputMethodMandoline(ui::internal::InputMethodDelegate* delegate);
  ~InputMethodMandoline() override;

 private:
  // Overridden from ui::InputMethod:
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  bool DispatchKeyEvent(const ui::KeyEvent& event) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void CancelComposition(const ui::TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsActive() override;
  bool IsCandidatePopupOpen() const override;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMandoline);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_
