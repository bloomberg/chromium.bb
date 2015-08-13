// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_RESULT_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_RESULT_FILTER_H_

#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// This interface is used by PasswordFormManager to filter results from the
// PasswordStore.
class StoreResultFilter {
 public:
  StoreResultFilter() {}
  virtual ~StoreResultFilter() {}

  // Removes from |results| all forms which should be ignored for any password
  // manager-related purposes, and returns the rest.
  virtual ScopedVector<autofill::PasswordForm> FilterResults(
      ScopedVector<autofill::PasswordForm> results) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreResultFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_RESULT_FILTER_H_
