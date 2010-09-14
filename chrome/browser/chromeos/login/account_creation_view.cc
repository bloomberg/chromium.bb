// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include "base/string_util.h"
#include "webkit/glue/form_data.h"

using webkit_glue::FormData;

namespace chromeos {

const char kCreateAccountFormName[] = "createaccount";
const char kEmailFieldName[] = "Email";
const char kDomainFieldName[] = "edk";

class AccountCreationTabContents : public WizardWebPageViewTabContents,
                                   public RenderViewHostDelegate::AutoFill {
 public:
  AccountCreationTabContents(Profile* profile,
                             SiteInstance* site_instance,
                             AccountCreationViewDelegate* delegate,
                             WebPageDelegate* page_delegate)
      : WizardWebPageViewTabContents(profile, site_instance, page_delegate),
        delegate_(delegate) {
  }

  virtual RenderViewHostDelegate::AutoFill* GetAutoFillDelegate() {
    return this;
  }

  virtual void FormSubmitted(const FormData& form) {
    if (UTF16ToASCII(form.name) == kCreateAccountFormName) {
      std::string user_name;
      std::string domain;
      for (size_t i = 0; i < form.fields.size(); i++) {
        std::string name = UTF16ToASCII(form.fields[i].name());
        if (name == kEmailFieldName) {
          user_name = UTF16ToASCII(form.fields[i].value());
        } else if (name == kDomainFieldName) {
          domain = UTF16ToASCII(form.fields[i].value());
        }
      }
      if (!user_name.empty()) {
        // We don't have password here because all password fields were
        // stripped. Overriding TabContents::PasswordFormsFound also makes no
        // sense because password value is always empty for account create page.
        delegate_->OnUserCreated(user_name + "@" + domain, "");
      }
    }
  }

  virtual void FormsSeen(const std::vector<FormData>& forms) {
  }

  virtual bool GetAutoFillSuggestions(
      int query_id, bool form_autofilled, const webkit_glue::FormField& field) {
    return false;
  }

  virtual bool FillAutoFillFormData(int query_id,
                                    const webkit_glue::FormData& form,
                                    int unique_id) {
    return false;
  }

  virtual void ShowAutoFillDialog() {}

 private:
  AccountCreationViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationTabContents);
};

///////////////////////////////////////////////////////////////////////////////
// AccountCreationDomView, public:

AccountCreationDomView::AccountCreationDomView() : delegate_(NULL) {
}

AccountCreationDomView::~AccountCreationDomView() {
}

void AccountCreationDomView::SetAccountCreationViewDelegate(
    AccountCreationViewDelegate* delegate) {
  delegate_ = delegate;
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationDomView, DOMView implementation:

TabContents* AccountCreationDomView::CreateTabContents(Profile* profile,
                                                       SiteInstance* instance) {
  return new AccountCreationTabContents(profile,
                                        instance,
                                        delegate_,
                                        page_delegate_);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:

AccountCreationView::AccountCreationView()
    : dom_view_(new AccountCreationDomView()) {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::SetAccountCreationViewDelegate(
    AccountCreationViewDelegate* delegate) {
  dom_view_->SetAccountCreationViewDelegate(delegate);
}

}  // namespace chromeos
