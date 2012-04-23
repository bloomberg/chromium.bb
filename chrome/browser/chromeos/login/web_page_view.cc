// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/web_page_view.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ipc/ipc_message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"

using base::TimeDelta;
using content::SiteInstance;
using views::Label;
using views::View;

namespace chromeos {

namespace {

// Spacing (vertical/horizontal) between controls.
const int kSpacing = 10;

// Time in ms after that waiting controls are shown on Start.
const int kStartDelayMs = 500;

// Time in ms after that waiting controls are hidden on Stop.
const int kStopDelayMs = 500;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebPageDomView, public:

WebPageDomView::WebPageDomView(content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
}

void WebPageDomView::SetWebContentsDelegate(
    content::WebContentsDelegate* delegate) {
  web_contents()->SetDelegate(delegate);
}

///////////////////////////////////////////////////////////////////////////////
// WebPageView, public:

WebPageView::WebPageView() : throbber_(NULL), connecting_label_(NULL) {}

WebPageView::~WebPageView() {}

void WebPageView::Init() {
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));
  set_border(CreateWizardBorder(&BorderDefinition::kScreenBorder));
  dom_view()->SetVisible(false);
  AddChildView(dom_view());

  throbber_ = CreateDefaultThrobber();
  AddChildView(throbber_);

  connecting_label_ = new views::Label();
  connecting_label_->SetText(
      l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  connecting_label_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  connecting_label_->SetVisible(false);
  AddChildView(connecting_label_);

  start_timer_.Start(FROM_HERE,
                     TimeDelta::FromMilliseconds(kStartDelayMs),
                     this,
                     &WebPageView::ShowWaitingControls);
}

void WebPageView::InitWebView(SiteInstance* site_instance) {
  dom_view()->CreateWebContentsWithSiteInstance(site_instance);
}

void WebPageView::LoadURL(const GURL& url) {
  dom_view()->LoadInitialURL(url);
}

void WebPageView::SetWebContentsDelegate(
    content::WebContentsDelegate* delegate) {
  dom_view()->SetWebContentsDelegate(delegate);
}

void WebPageView::ShowPageContent() {
  // TODO(nkostylev): Show throbber as an overlay until page has been rendered.
  start_timer_.Stop();
  if (!stop_timer_.IsRunning()) {
    stop_timer_.Start(FROM_HERE,
                      TimeDelta::FromMilliseconds(kStopDelayMs),
                      this,
                      &WebPageView::ShowRenderedPage);
  }
}

///////////////////////////////////////////////////////////////////////////////
// WebPageView, private:

void WebPageView::ShowRenderedPage() {
  throbber_->Stop();
  connecting_label_->SetVisible(false);
  dom_view()->SetVisible(true);
}

void WebPageView::ShowWaitingControls() {
  throbber_->Start();
  connecting_label_->SetVisible(true);
}

///////////////////////////////////////////////////////////////////////////////
// WebPageView, views::View implementation:

void WebPageView::Layout() {
  dom_view()->SetBoundsRect(GetContentsBounds());
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
