// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/credentials_filter.h"

namespace password_manager {

// Stub of the CredentialsFilter API, to be used in tests. This filter does
// not filter out anything.
class StubCredentialsFilter : public CredentialsFilter {
 public:
  StubCredentialsFilter();

  ~StubCredentialsFilter() override;

  // CredentialsFilter
  std::vector<std::unique_ptr<autofill::PasswordForm>> FilterResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results)
      const override;
  bool ShouldSave(const autofill::PasswordForm& form,
                  const GURL& main_frame_url) const override;
  void ReportFormLoginSuccess(
      const PasswordFormManager& form_manager) const override;

  // A version of FilterResult without moveable arguments, which cannot be
  // mocked in GMock. StubCredentialsFilter::FilterResults(arg) calls
  // FilterResultsPtr(&arg).
  virtual void FilterResultsPtr(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* results) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubCredentialsFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_CREDENTIALS_FILTER_H_
