// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakePasswordGenerationDriver
    : public autofill::mojom::PasswordGenerationDriver {
 public:
  FakePasswordGenerationDriver();

  ~FakePasswordGenerationDriver() override;

  void BindRequest(
      autofill::mojom::PasswordGenerationDriverAssociatedRequest request);

  void Flush();

  bool called_automatic_generation_status_changed_true() const {
    return called_automatic_generation_status_changed_true_;
  }

  bool called_generation_available_for_form() const {
    return called_generation_available_for_form_;
  }

  bool called_password_generation_rejected_by_typing() const {
    return called_password_generation_rejected_by_typing_;
  }

  void reset_called_automatic_generation_status_changed_true() {
    called_automatic_generation_status_changed_true_ = false;
  }

  void reset_called_generation_available_for_form() {
    called_generation_available_for_form_ = false;
  }

  void reset_called_password_generation_rejected_by_typing() {
    called_password_generation_rejected_by_typing_ = false;
  }

  // TODO(crbug.com/851021): move all the methods to GMock.
  // autofill::mojom::PasswordGenerationDriver:
  MOCK_METHOD1(PresaveGeneratedPassword,
               void(const autofill::PasswordForm& password_form));
  MOCK_METHOD1(PasswordNoLongerGenerated,
               void(const autofill::PasswordForm& password_form));
  MOCK_METHOD2(ShowPasswordEditingPopup,
               void(const gfx::RectF& bounds,
                    const autofill::PasswordForm& form));

 private:
  // autofill::mojom::PasswordManagerClient:
  void AutomaticGenerationStatusChanged(
      bool available,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data)
      override;

  void GenerationAvailableForForm(const autofill::PasswordForm& form) override;

  void PasswordGenerationRejectedByTyping() override;

  // Records whether AutomaticGenerationStatusChanged(true) gets called.
  bool called_automatic_generation_status_changed_true_ = false;

  // Records whether GenerationAvailableForForm() gets called.
  bool called_generation_available_for_form_ = false;

  // Records whether PasswordGenerationRejecteByTyping() gets called.
  bool called_password_generation_rejected_by_typing_ = false;

  mojo::AssociatedBinding<autofill::mojom::PasswordGenerationDriver> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakePasswordGenerationDriver);
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
