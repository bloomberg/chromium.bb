// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include "base/string_util.h"
#include "chrome/common/autofill_messages.h"
#include "webkit/glue/form_data.h"

using webkit_glue::FormData;

namespace chromeos {

const char kCreateAccountFormName[] = "createaccount";
const char kEmailFieldName[] = "Email";
const char kDomainFieldName[] = "edk";

class AccountCreationTabContents : public WizardWebPageViewTabContents {
 public:
  AccountCreationTabContents(Profile* profile,
                             SiteInstance* site_instance,
                             AccountCreationViewDelegate* delegate,
                             WebPageDelegate* page_delegate)
      : WizardWebPageViewTabContents(profile, site_instance, page_delegate),
        delegate_(delegate) {
  }

  // Overriden from TabContents.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(AccountCreationTabContents, message)
      IPC_MESSAGE_HANDLER(AutoFillHostMsg_FormSubmitted, OnFormSubmitted)
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_FormsSeen, )
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_QueryFormFieldAutoFill, )
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_ShowAutoFillDialog, )
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_FillAutoFillFormData, )
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_DidFillAutoFillFormData, )
      IPC_MESSAGE_HANDLER_GENERIC(AutoFillHostMsg_DidShowAutoFillSuggestions, )
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    if (handled)
      return true;
    return TabContents::OnMessageReceived(message);
  }

 private:
  void OnFormSubmitted(const FormData& form) {
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
