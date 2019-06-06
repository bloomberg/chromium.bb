// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
struct Suggestion;
}

namespace content {
class WebContents;
}

class ManualFillingController;

class TouchToFillController
    : public base::SupportsWeakPtr<TouchToFillController>,
      public content::WebContentsUserData<TouchToFillController>,
      public AccessoryController {
 public:
  // Returns a reference to the unique TouchToFillController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called. Only valid to be called if
  // TouchToFillController::AllowedForWebContents(web_contents).
  static TouchToFillController* GetOrCreate(content::WebContents* web_contents);

  // Allow injecting a custom ManualFillingController for testing.
  static std::unique_ptr<TouchToFillController> CreateForTesting(
      base::WeakPtr<ManualFillingController> mf_controller);

  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController() override;

  // Returns true if the touch to fill controller may exist for |web_contents|.
  // Otherwise it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Instructs the controller to show the provided |suggestions| to the user.
  void Show(const std::vector<autofill::Suggestion>& suggestions);

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;

 private:
  friend class content::WebContentsUserData<TouchToFillController>;

  explicit TouchToFillController(content::WebContents* web_contents);

  // Constructor corresponding to CreateForTesting().
  explicit TouchToFillController(
      base::WeakPtr<ManualFillingController> mf_controller);

  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
