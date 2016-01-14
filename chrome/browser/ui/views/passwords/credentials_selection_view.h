// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
}

class ManagePasswordsBubbleModel;

// A view where the user can select a credential.
class CredentialsSelectionView : public views::View {
 public:
  CredentialsSelectionView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      const std::vector<const autofill::PasswordForm*>& password_forms,
      const base::string16& best_matched_username);
  ~CredentialsSelectionView() override;

  // This methods also reports a user action.
  const autofill::PasswordForm* GetSelectedCredentials();

 private:
  views::Combobox* GenerateUsernameCombobox(
      const std::vector<const autofill::PasswordForm*>& forms,
      const base::string16& best_matched_username);
  void ReportUserActionOnce(bool was_update_rejected, int selected_index);

  const std::vector<const autofill::PasswordForm*>& password_forms_;
  views::Combobox* combobox_;
  int default_index_;
  bool is_default_best_match_;
  bool is_default_preferred_;
  bool action_reported_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsSelectionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_
