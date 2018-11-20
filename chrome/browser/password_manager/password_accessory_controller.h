// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/filling_status.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

// Interface for password-specific keyboard accessory controller between the
// ManualFillingController and PasswordManagerClient.
//
// There is a single instance per WebContents that can be accessed by calling:
//     PasswordAccessoryController::GetOrCreate(web_contents);
// On the first call, an instance is attached to |web_contents|, so it can be
// returned by subsequent calls.
class PasswordAccessoryController
    : public base::SupportsWeakPtr<PasswordAccessoryController> {
 public:
  PasswordAccessoryController() = default;
  virtual ~PasswordAccessoryController() = default;

  // Returns true if the accessory controller may exist for |web_contents|.
  // Otherwise (e.g. if VR is enabled), it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Returns a reference to the unique PasswordAccessoryController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called. Only valid to be called if
  // |PasswordAccessoryController::AllowedForWebContents(web_contents)|.
  static PasswordAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the unique PasswordAccessoryController associated
  // with |web_contents|. Returns null if no such instance exists.
  static PasswordAccessoryController* GetIfExisting(
      content::WebContents* web_contents);

  // -----------------------------
  // Methods called by the client:
  // -----------------------------

  // Saves credentials for an origin so that they can be used in the sheet.
  virtual void SavePasswordsForOrigin(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const url::Origin& origin) = 0;

  // Notifies the view that automatic password generation status changed.
  virtual void OnAutomaticGenerationStatusChanged(
      bool available,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver) = 0;

  // Completes a filling attempt by recording metrics, giving feedback to the
  // user and dismissing the accessory sheet.
  virtual void OnFilledIntoFocusedField(autofill::FillingStatus status) = 0;

  // Makes sure, that all shown suggestions are appropriate for the currently
  // focused field and for fields that lost the focus. If a field lost focus,
  // |is_fillable| will be false.
  virtual void RefreshSuggestionsForField(const url::Origin& origin,
                                          bool is_fillable,
                                          bool is_password_field) = 0;

  // Reacts to a navigation on the main frame, e.g. by clearing caches.
  virtual void DidNavigateMainFrame() = 0;

  // Requests to show the accessory bar. The accessory will only be shown
  // when the keyboard becomes visible.
  virtual void ShowWhenKeyboardIsVisible() = 0;

  // Requests to hide the accessory. This hides both the accessory sheet
  // (if open) and the accessory bar.
  virtual void Hide() = 0;

  // --------------------------
  // Methods called by UI code:
  // --------------------------

  // Gets an icon for the currently focused frame and passes it to
  // |icon_callback|. The callback is invoked with an image unless an icon for
  // a new origin was called. In the latter case, the callback is dropped.
  // The callback is called with an |IsEmpty()| image if there is no favicon.
  virtual void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) = 0;

  // Called by the UI code to request that |text_to_fill| is to be filled into
  // the currently focused field.
  virtual void OnFillingTriggered(bool is_password,
                                  const base::string16& text_to_fill) = 0;

  // Called by the UI code because a user triggered the |selected_option|,
  // such as "Manage passwords..."
  // TODO(crbug.com/905669): Replace the string param with an enum to indicate
  // the selected option.
  virtual void OnOptionSelected(
      const base::string16& selected_option) const = 0;

  // Called by the UI code to signal that the user requested password
  // generation. This should prompt a modal dialog with the generated password.
  virtual void OnGenerationRequested() = 0;

  // Called from the modal dialog if the user accepted the generated password.
  virtual void GeneratedPasswordAccepted(const base::string16& password) = 0;

  // Called from the modal dialog if the user rejected the generated password.
  virtual void GeneratedPasswordRejected() = 0;

  // -----------------
  // Member accessors:
  // -----------------

  virtual gfx::NativeWindow native_window() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
