// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/dom_view.h"

namespace {

const int kCornerRadius = 12;
const int kShadow = 10;
const int kPadding = 30;
const SkColor kBackground = SK_ColorWHITE;
const SkColor kShadowColor = 0x40223673;

}  // namespace

class DelegatedDOMView : public DOMView {
 public:
  void SetTabContentsDelegate(TabContentsDelegate* delegate) {
    tab_contents_->set_delegate(delegate);
  }
};

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:
AccountCreationView::AccountCreationView(chromeos::ScreenObserver* observer)
    : observer_(observer),
      dom_(NULL),
      site_instance_(NULL) {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::Init() {
  // Use rounded rect background.
  views::Painter* painter = new chromeos::RoundedRectPainter(
      0, 0x00000000,              // no padding
      kShadow, kShadowColor,      // gradient shadow
      kCornerRadius,              // corner radius
      kBackground, kBackground);  // backgound without gradient
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  dom_ = new DelegatedDOMView();
  AddChildView(dom_);

  GURL url("https://www.google.com/accounts/NewAccount?btmpl=tier2_mobile");
  Profile* profile = ProfileManager::GetLoginWizardProfile();
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(profile, url);
  dom_->Init(profile, site_instance_);
  dom_->SetTabContentsDelegate(this);
  dom_->LoadURL(url);
}

void AccountCreationView::UpdateLocalizedStrings() {
  // TODO(avayvod): Reload account creation page with the right language.
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::View overrides:
gfx::Size AccountCreationView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void AccountCreationView::Layout() {
  dom_->SetBounds(kPadding,
                  kPadding,
                  width() - kPadding * 2,
                  height() - kPadding * 2);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, TabContentsDelegate implementation:
void AccountCreationView::OpenURLFromTab(TabContents* source,
                                         const GURL& url,
                                         const GURL& referrer,
                                         WindowOpenDisposition disposition,
                                         PageTransition::Type transition) {
  LOG(INFO) << "Url: " << url.spec().c_str() << "\n";
  LOG(INFO) << "Referrer: " << referrer.spec().c_str() << "\n";
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, private:
void AccountCreationView::CreateAccount() {
  // TODO(avayvod): Implement account creation.
  if (observer_) {
    observer_->OnExit(chromeos::ScreenObserver::ACCOUNT_CREATED);
  }
}
