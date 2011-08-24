// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_view.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/border.h"
#include "views/layout/layout_constants.h"

namespace chromeos {

namespace {

// Layout constants.
const int kBorderSize = 30;

// Renders the registration page.
class EnrollmentDomView : public WebPageDomView,
                          public TabContentsDelegate {
 public:
  EnrollmentDomView() {}
  virtual ~EnrollmentDomView() {
  }

 protected:
  // DomView implementation:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance) {
    TabContents* contents = new WizardWebPageViewTabContents(profile,
                                                             instance,
                                                             page_delegate_);
    contents->set_delegate(this);
    return contents;
  }

  // TabContentsDelegate implementation:
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type) {
    return false;
  }
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentDomView);
};

}  // namespace

EnterpriseEnrollmentView::EnterpriseEnrollmentView(
    EnterpriseEnrollmentScreenActor::Controller* controller)
    : controller_(controller), actor_(NULL) {}

EnterpriseEnrollmentView::~EnterpriseEnrollmentView() {}

void EnterpriseEnrollmentView::Init() {
  // Use rounded rect background.
  views::Painter* painter =
      CreateWizardPainter(&BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  // Create the view that hosts the enrollment page.
  enrollment_page_view_ = new EnrollmentDomView();
  enrollment_page_view_->set_border(
      views::Border::CreateEmptyBorder(kBorderSize, kBorderSize,
                                       kBorderSize, kBorderSize));

  AddChildView(enrollment_page_view_);

  // Load the enrollment page.
  Profile* profile = ProfileManager::GetDefaultProfile();
  GURL url(chrome::kChromeUIEnterpriseEnrollmentURL);
  enrollment_page_view_->Init(
      profile, SiteInstance::CreateSiteInstanceForURL(profile, url));

  enrollment_page_view_->LoadURL(url);
  actor_ = static_cast<EnterpriseEnrollmentUI*>(
      enrollment_page_view_->tab_contents()->web_ui())->GetActor();
  actor_->SetController(controller_);
}

EnterpriseEnrollmentScreenActor* EnterpriseEnrollmentView::GetActor() {
  return actor_;
}

void EnterpriseEnrollmentView::Layout() {
  enrollment_page_view_->SetBoundsRect(GetContentsBounds());
}

void EnterpriseEnrollmentView::RequestFocus() {
  enrollment_page_view_->RequestFocus();
}

}  // namespace chromeos
