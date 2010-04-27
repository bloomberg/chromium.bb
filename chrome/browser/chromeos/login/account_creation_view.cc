// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/bindings_policy.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ipc/ipc_message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "webkit/glue/form_data.h"

using base::TimeDelta;
using views::Label;
using views::Throbber;
using views::View;
using webkit_glue::FormData;

// Spacing (vertical/horizontal) between controls.
const int kSpacing = 10;

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

// Time in ms after that waiting controls are shown on Start.
const int kStartDelayMs = 500;

// Time in ms after that waiting controls are hidden Stop.
const int kStopDelayMs = 500;

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
        // stripped. Overriding TabContents::PasswordFormsSeen also has no sense
        // becuase password value is always empty for account create page.
        delegate_->OnUserCreated(user_name + "@" + domain, "");
      }
    }
  }

  virtual void FormsSeen(const std::vector<FormData>& forms) {
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

  virtual void DocumentLoadedInFrame() {
    delegate_->OnPageLoaded();
  }

  virtual void OnContentBlocked(ContentSettingsType type) {
    delegate_->OnPageLoadFailed("");
  }

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

void AccountCreationDomView::SetTabContentsDelegate(
    TabContentsDelegate* delegate) {
  tab_contents_->set_delegate(delegate);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationDomView, DOMView implementation:

TabContents* AccountCreationDomView::CreateTabContents(Profile* profile,
                                                       SiteInstance* instance) {
  return new AccountCreationTabContents(profile, instance, delegate_);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:

AccountCreationView::AccountCreationView()
    : dom_view_(new AccountCreationDomView()),
      throbber_(NULL),
      connecting_label_(NULL) {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::Init() {
  chromeos::CreateWizardBorder(
      &chromeos::BorderDefinition::kScreenBorder)->GetInsets(&insets_);
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));
  dom_view_->SetVisible(false);
  AddChildView(dom_view_);

  throbber_ = new views::Throbber(kThrobberFrameMs, false);
  throbber_->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  AddChildView(throbber_);

  connecting_label_ = new views::Label();
  connecting_label_->SetText(l10n_util::GetString(IDS_LOAD_STATE_CONNECTING));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  connecting_label_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  connecting_label_->SetVisible(false);
  AddChildView(connecting_label_ );

  start_timer_.Start(TimeDelta::FromMilliseconds(kStartDelayMs),
                     this,
                     &AccountCreationView::ShowWaitingControls);
}

void AccountCreationView::InitDOM(Profile* profile,
                                  SiteInstance* site_instance) {
  dom_view_->Init(profile, site_instance);
}

void AccountCreationView::LoadURL(const GURL& url) {
  dom_view_->LoadURL(url);
}

void AccountCreationView::SetTabContentsDelegate(
    TabContentsDelegate* delegate) {
  dom_view_->SetTabContentsDelegate(delegate);
}

void AccountCreationView::SetAccountCreationViewDelegate(
    AccountCreationViewDelegate* delegate) {
  dom_view_->SetAccountCreationViewDelegate(delegate);
}

void AccountCreationView::ShowPageContent() {
  // TODO(nkostylev): Show throbber as an overlay until page has been rendered.
  start_timer_.Stop();
  if (!stop_timer_.IsRunning()) {
    stop_timer_.Start(TimeDelta::FromMilliseconds(kStopDelayMs),
                        this,
                        &AccountCreationView::ShowRenderedPage);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, private:

void AccountCreationView::ShowRenderedPage() {
  throbber_->Stop();
  connecting_label_->SetVisible(false);
  dom_view_->SetVisible(true);
}

void AccountCreationView::ShowWaitingControls() {
  throbber_->Start();
  connecting_label_->SetVisible(true);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::View implementation:

void AccountCreationView::Layout() {
  dom_view_->SetBounds(insets_.left(),
                       insets_.top(),
                       bounds().width() - insets_.left() - insets_.right(),
                       bounds().height() - insets_.top() - insets_.bottom());
  int y = height() / 2  - throbber_->GetPreferredSize().height() / 2;
  throbber_->SetBounds(
      width() / 2 - throbber_->GetPreferredSize().width() / 2,
      y,
      throbber_->GetPreferredSize().width(),
      throbber_->GetPreferredSize().height());
  connecting_label_->SetBounds(
      width() / 2 - connecting_label_->GetPreferredSize().width() / 2,
      y + throbber_->GetPreferredSize().height() + kSpacing,
      connecting_label_->GetPreferredSize().width(),
      connecting_label_->GetPreferredSize().height());
}

}  // namespace chromeos
