// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_

#include "chrome/browser/autofill/credit_card_accessory_controller.h"

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace content {
class WebContents;
}

class ManualFillingController;

namespace autofill {

class CreditCardAccessoryControllerImpl : public CreditCardAccessoryController {
 public:
  CreditCardAccessoryControllerImpl(PersonalDataManager* personal_data_manager,
                                    content::WebContents* web_contents);
  ~CreditCardAccessoryControllerImpl() override;

  // AccessoryController:
  // TODO(crbug.com/902425): Implement filling logic.
  void OnFillingTriggered(const UserInfo::Field& selection) override {}
  void OnOptionSelected(AccessoryAction selected_action) override;

  // CreditCardAccessoryController:
  void RefreshSuggestionsForField() override;

  void SetManualFillingControllerForTesting(
      base::WeakPtr<ManualFillingController> controller);

 private:
  const std::vector<CreditCard*> GetSuggestions();
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  const PersonalDataManager* personal_data_manager_;
  content::WebContents* web_contents_;
  base::WeakPtr<ManualFillingController> mf_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_
