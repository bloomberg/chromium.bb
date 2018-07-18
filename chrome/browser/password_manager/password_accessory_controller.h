// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

class PasswordAccessoryViewInterface;
class PasswordGenerationDialogViewInterface;

// The controller for the view located below the keyboard accessory.
// Upon creation, it creates (and owns) a corresponding PasswordAccessoryView.
// This view will be provided with data and will notify this controller about
// interactions (like requesting to fill a password suggestions).
//
// Create it for a WebContents instance by calling:
//     PasswordAccessoryController::CreateForWebContents(web_contents);
// After that, it's attached to the |web_contents| instance and can be retrieved
// by calling:
//     PasswordAccessoryController::FromWebContents(web_contents);
// Any further calls to |CreateForWebContents| will be a noop.
//
// TODO(fhorschig): This class currently only supports credentials originating
// from the main frame. Supporting iframes is intended: https://crbug.com/854150
class PasswordAccessoryController
    : public content::WebContentsUserData<PasswordAccessoryController> {
 public:
  using CreateDialogFactory = base::RepeatingCallback<std::unique_ptr<
      PasswordGenerationDialogViewInterface>(PasswordAccessoryController*)>;
  ~PasswordAccessoryController() override;

  // Notifies the view about credentials to be displayed.
  void OnPasswordsAvailable(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const GURL& origin);

  // Send empty suggestions and default options to the view.
  void DidNavigateMainFrame();

  // Notifies the view that automatic password generation status changed.
  void OnAutomaticGenerationStatusChanged(
      bool available,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver);

  // Called by the UI code to request that |textToFill| is to be filled into the
  // currently focused field.
  void OnFillingTriggered(bool is_password,
                          const base::string16& textToFill) const;

  // Called by the UI code because a user triggered the |selectedOption|.
  void OnOptionSelected(const base::string16& selectedOption) const;

  // Called by the UI code to signal that the user requested password
  // generation. This should prompt a modal dialog with the generated password.
  void OnGenerationRequested();

  // Called from the modal dialog if the user accepted the generated password.
  void GeneratedPasswordAccepted(const base::string16& password);

  // Called from the modal dialog if the user rejected the generated password.
  void GeneratedPasswordRejected();

  // Called from the modal dialog when the user taps on the link contained
  // in the explanation text that leads to the saved passwords.
  void OnSavedPasswordsLinkClicked();

  // The web page view containing the focused field.
  gfx::NativeView container_view() const;

  gfx::NativeWindow native_window() const;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a fake/mock view.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> test_view,
      CreateDialogFactory create_dialog_callback);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  PasswordAccessoryViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)

 private:
  // Data including the form and field for which generation was requested,
  // their signatures and the maximum password size.
  struct GenerationElementData;

  // Required for construction via |CreateForWebContents|:
  explicit PasswordAccessoryController(content::WebContents* contents);
  friend class content::WebContentsUserData<PasswordAccessoryController>;

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryController(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> view,
      CreateDialogFactory create_dialog_callback);

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // Data for the generation element used to generate the password.
  std::unique_ptr<GenerationElementData> generation_element_data_;

  // Password manager driver for the target frame used for password generation.
  base::WeakPtr<password_manager::PasswordManagerDriver> target_frame_driver_;

  // Modal dialog view meant to display the generated password.
  std::unique_ptr<PasswordGenerationDialogViewInterface> dialog_view_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<PasswordAccessoryViewInterface> view_;

  // Creation callback for the modal dialog view meant to facilitate testing.
  CreateDialogFactory create_dialog_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
