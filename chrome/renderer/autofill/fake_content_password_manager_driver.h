// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_CONTENT_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_CONTENT_PASSWORD_MANAGER_DRIVER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class FakeContentPasswordManagerDriver
    : public autofill::mojom::PasswordManagerDriver {
 public:
  FakeContentPasswordManagerDriver();

  ~FakeContentPasswordManagerDriver() override;

  void BindRequest(autofill::mojom::PasswordManagerDriverRequest request);

  bool called_show_pw_suggestions() const {
    return called_show_pw_suggestions_;
  }

  int show_pw_suggestions_key() const { return show_pw_suggestions_key_; }

  const base::Optional<base::string16>& show_pw_suggestions_username() const {
    return show_pw_suggestions_username_;
  }

  int show_pw_suggestions_options() const {
    return show_pw_suggestions_options_;
  }

  void reset_show_pw_suggestions() {
    called_show_pw_suggestions_ = false;
    show_pw_suggestions_key_ = -1;
    show_pw_suggestions_username_ = base::nullopt;
    show_pw_suggestions_options_ = -1;
  }

  bool called_show_not_secure_warning() const {
    return called_show_not_secure_warning_;
  }

  bool called_password_form_submitted() const {
    return called_password_form_submitted_;
  }

  const base::Optional<autofill::PasswordForm>& password_form_submitted()
      const {
    return password_form_submitted_;
  }

  bool called_inpage_navigation() const { return called_inpage_navigation_; }

  const base::Optional<autofill::PasswordForm>&
  password_form_inpage_navigation() const {
    return password_form_inpage_navigation_;
  }

  bool called_password_forms_rendered() const {
    return called_password_forms_rendered_;
  }

  const base::Optional<std::vector<autofill::PasswordForm>>&
  password_forms_rendered() const {
    return password_forms_rendered_;
  }

  void reset_password_forms_rendered() {
    called_password_forms_rendered_ = false;
    password_forms_rendered_ = base::nullopt;
  }

  bool called_record_save_progress() const {
    return called_record_save_progress_;
  }

  bool called_save_generation_field() const {
    return called_save_generation_field_;
  }

  const base::Optional<base::string16>& save_generation_field() const {
    return save_generation_field_;
  }

  void reset_save_generation_field() {
    called_save_generation_field_ = false;
    save_generation_field_ = base::nullopt;
  }

  bool called_password_no_longer_generated() const {
    return called_password_no_longer_generated_;
  }

  void reset_called_password_no_longer_generated() {
    called_password_no_longer_generated_ = false;
  }

  bool called_presave_generated_password() const {
    return called_presave_generated_password_;
  }

  void reset_called_presave_generated_password() {
    called_presave_generated_password_ = false;
  }

 private:
  // mojom::PasswordManagerDriver:
  void PasswordFormsParsed(
      const std::vector<autofill::PasswordForm>& forms) override;

  void PasswordFormsRendered(
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading) override;

  void PasswordFormSubmitted(
      const autofill::PasswordForm& password_form) override;

  void InPageNavigation(const autofill::PasswordForm& password_form) override;

  void PresaveGeneratedPassword(
      const autofill::PasswordForm& password_form) override;

  void PasswordNoLongerGenerated(
      const autofill::PasswordForm& password_form) override;

  void ShowPasswordSuggestions(int key,
                               base::i18n::TextDirection text_direction,
                               const base::string16& typed_username,
                               int options,
                               const gfx::RectF& bounds) override;

  void ShowNotSecureWarning(base::i18n::TextDirection text_direction,
                            const gfx::RectF& bounds) override;

  void RecordSavePasswordProgress(const std::string& log) override;

  void SaveGenerationFieldDetectedByClassifier(
      const autofill::PasswordForm& password_form,
      const base::string16& generation_field) override;

  // Records whether ShowPasswordSuggestions() gets called.
  bool called_show_pw_suggestions_ = false;
  // Records data received via ShowPasswordSuggestions() call.
  int show_pw_suggestions_key_ = -1;
  base::Optional<base::string16> show_pw_suggestions_username_;
  int show_pw_suggestions_options_ = -1;
  // Records whether ShowNotSecureWarning() gets called.
  bool called_show_not_secure_warning_ = false;
  // Records whether PasswordFormSubmitted() gets called.
  bool called_password_form_submitted_ = false;
  // Records data received via PasswordFormSubmitted() call.
  base::Optional<autofill::PasswordForm> password_form_submitted_;
  // Records whether InPageNavigation() gets called.
  bool called_inpage_navigation_ = false;
  // Records data received via InPageNavigation() call.
  base::Optional<autofill::PasswordForm> password_form_inpage_navigation_;
  // Records whether PasswordFormsRendered() gets called.
  bool called_password_forms_rendered_ = false;
  // Records data received via PasswordFormsRendered() call.
  base::Optional<std::vector<autofill::PasswordForm>> password_forms_rendered_;
  // Records whether RecordSavePasswordProgress() gets called.
  bool called_record_save_progress_ = false;
  // Records whether SaveGenerationFieldDetectedByClassifier() gets called.
  bool called_save_generation_field_ = false;
  // Records data received via SaveGenerationFieldDetectedByClassifier() call.
  base::Optional<base::string16> save_generation_field_;
  // Records whether PasswordNoLongerGenerated() gets called.
  bool called_password_no_longer_generated_ = false;
  // Records whether PresaveGeneratedPassword() gets called.
  bool called_presave_generated_password_ = false;

  mojo::BindingSet<autofill::mojom::PasswordManagerDriver> bindings_;
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_CONTENT_PASSWORD_MANAGER_DRIVER_H_
