// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include "base/callback.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/bindings_policy.h"
#include "gfx/canvas.h"
#include "ipc/ipc_message.h"
#include "views/border.h"
#include "webkit/glue/form_field_values.h"

namespace chromeos {

const char kCreateAccountFormName[] = "createaccount";
const char kEmailFieldName[] = "Email";
const char kDomainFieldName[] = "edk";

class AccountCreationTabContents : public TabContents,
                                   public RenderViewHostDelegate::AutoFill {
 public:
  AccountCreationTabContents(Profile* profile,
                             SiteInstance* site_instance,
                             AccountCreationViewDelegate* delegate)
      : TabContents(profile, site_instance, MSG_ROUTING_NONE, NULL),
        delegate_(delegate) {
  }

  virtual RenderViewHostDelegate::AutoFill* GetAutoFillDelegate() {
    return this;
  }

  virtual void FormFieldValuesSubmitted(
        const webkit_glue::FormFieldValues& form) {
    if (UTF16ToASCII(form.form_name) == kCreateAccountFormName) {
      std::string user_name;
      std::string domain;
      for (size_t i = 0; i < form.elements.size(); i++) {
        std::string name = UTF16ToASCII(form.elements[i].name());
        if (name == kEmailFieldName) {
          user_name = UTF16ToASCII(form.elements[i].value());
        } else if (name == kDomainFieldName) {
          domain = UTF16ToASCII(form.elements[i].value());
        }
      }
      if (!user_name.empty()) {
        // We don't have password here because all password fields were
        // stripped. Overriding TabContents::PasswordFormsSeen also has no sense
        // becuase password value is always empty for account create page.
        delegate_->OnUserCreated(user_name + "@" + domain, "");
      }
    }
  }

  virtual void FormsSeen(
      const std::vector<webkit_glue::FormFieldValues>& forms) {
  }

  virtual bool GetAutoFillSuggestions(
      int query_id, const webkit_glue::FormField& field) {
    return false;
  }

  virtual bool FillAutoFillFormData(int query_id,
                                    const webkit_glue::FormData& form,
                                    const string16& name,
                                    const string16& label) {
    return false;
  }

  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      bool is_main_frame,
      int error_code,
      const GURL& url,
      bool showing_repost_interstitial) {
    delegate_->OnPageLoadFailed(url.spec());
  }

  virtual void DidDisplayInsecureContent() {
    delegate_->OnPageLoadFailed("");
  }

  virtual void DidRunInsecureContent(const std::string& security_origin) {
    delegate_->OnPageLoadFailed(security_origin);
  }

  virtual void OnContentBlocked(ContentSettingsType type) {
    delegate_->OnPageLoadFailed("");
  }

 private:
  AccountCreationViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationTabContents);
};

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:
AccountCreationView::AccountCreationView()  : delegate_(NULL) {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::Init() {
  // Use rounded rect background.
  views::Border* border = chromeos::CreateWizardBorder(
      &chromeos::BorderDefinition::kScreenBorder);
  set_border(border);
}

void AccountCreationView::InitDOM(Profile* profile,
                                  SiteInstance* site_instance) {
  DOMView::Init(profile, site_instance);
}

TabContents* AccountCreationView::CreateTabContents(Profile* profile,
                                                    SiteInstance* instance) {
  return new AccountCreationTabContents(profile, instance, delegate_);
}

void AccountCreationView::SetTabContentsDelegate(
    TabContentsDelegate* delegate) {
  tab_contents_->set_delegate(delegate);
}

void AccountCreationView::SetAccountCreationViewDelegate(
    AccountCreationViewDelegate* delegate) {
  delegate_ = delegate;
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::View implementation:
void AccountCreationView::Paint(gfx::Canvas* canvas) {
  PaintBorder(canvas);
  DOMView::Paint(canvas);
}

}  // namespace chromeos
