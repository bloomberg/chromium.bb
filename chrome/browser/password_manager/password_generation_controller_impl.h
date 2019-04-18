// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_generation_controller.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents_user_data.h"

class ManualFillingController;
class PasswordGenerationDialogViewInterface;

// In its current state, this class is not involved in the editing flow for
// a generated password.
//
// Use either PasswordGenerationController::GetOrCreate or
// PasswordGenerationController::GetIfExisting to obtain instances of this
// class.
class PasswordGenerationControllerImpl
    : public PasswordGenerationController,
      public content::WebContentsUserData<PasswordGenerationControllerImpl> {
 public:
  using CreateDialogFactory = base::RepeatingCallback<std::unique_ptr<
      PasswordGenerationDialogViewInterface>(PasswordGenerationController*)>;

  ~PasswordGenerationControllerImpl() override;

  // PasswordGenerationController:
  void OnAutomaticGenerationAvailable(
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver)
      override;
  void OnGenerationElementLostFocus() override;
  void OnGenerationRequested() override;
  void GeneratedPasswordAccepted(
      const base::string16& password,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver) override;
  void GeneratedPasswordRejected() override;
  gfx::NativeWindow top_level_native_window() const override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting mocks for
  // testing.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateDialogFactory create_dialog_callback);

 private:
  // Data including the form and field for which generation was requested,
  // their signatures and the maximum password size.
  struct GenerationElementData;

  friend class content::WebContentsUserData<PasswordGenerationControllerImpl>;

  explicit PasswordGenerationControllerImpl(content::WebContents* web_contents);

  // Constructor that allows to inject a mock or fake view.
  PasswordGenerationControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateDialogFactory create_dialog_callback);

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // Data for the generation element used to generate the password.
  std::unique_ptr<GenerationElementData> generation_element_data_;

  // Password manager driver for the target frame used for password generation.
  base::WeakPtr<password_manager::PasswordManagerDriver> target_frame_driver_;

  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> manual_filling_controller_;

  // Modal dialog view meant to display the generated password.
  std::unique_ptr<PasswordGenerationDialogViewInterface> dialog_view_;

  // Creation callback for the modal dialog view meant to facilitate testing.
  CreateDialogFactory create_dialog_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_IMPL_H_
