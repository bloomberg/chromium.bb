// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_

#include "base/macros.h"
#include "components/autofill/core/common/form_data.h"

namespace password_manager {

// This class helps with filling the observed form and with saving/updating the
// stored information about it. It is aimed to replace PasswordFormManager and
// to be renamed in new Password Manager design. Details
// go/new-cpm-design-refactoring.
class NewPasswordFormManager {
 public:
  explicit NewPasswordFormManager(const autofill::FormData& observed_form);

  ~NewPasswordFormManager();

  // Compares |observed_form_| with |form| and returns true if they are the
  // same.
  bool DoesManage(const autofill::FormData& form) const;

 private:
  const autofill::FormData observed_form_;

  DISALLOW_COPY_AND_ASSIGN(NewPasswordFormManager);
};

}  // namespace  password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
