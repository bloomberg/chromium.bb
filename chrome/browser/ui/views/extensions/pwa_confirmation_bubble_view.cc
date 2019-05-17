// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/pwa_confirmation_bubble_view.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

PWAConfirmationBubbleView* g_bubble_ = nullptr;

}  // namespace

PWAConfirmationBubbleView::PWAConfirmationBubbleView(
    views::View* anchor_view,
    views::Button* highlight_button,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : LocationBarBubbleDelegateView(anchor_view, gfx::Point(), nullptr),
      pwa_confirmation_(this, std::move(web_app_info), std::move(callback)) {
  SetHighlightedButton(highlight_button);
}

bool PWAConfirmationBubbleView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PWAConfirmationBubbleView::GetWindowTitle() const {
  return PWAConfirmation::GetWindowTitle();
}

base::string16 PWAConfirmationBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return PWAConfirmation::GetDialogButtonLabel(button);
}

views::View* PWAConfirmationBubbleView::GetInitiallyFocusedView() {
  return PWAConfirmation::GetInitiallyFocusedView(this);
}

void PWAConfirmationBubbleView::WindowClosing() {
  DCHECK_EQ(g_bubble_, this);
  g_bubble_ = nullptr;
  pwa_confirmation_.WindowClosing();
}

bool PWAConfirmationBubbleView::Accept() {
  pwa_confirmation_.Accept();
  return true;
}

namespace chrome {

void ShowPWAInstallBubble(content::WebContents* web_contents,
                          std::unique_ptr<WebApplicationInfo> web_app_info,
                          AppInstallationAcceptanceCallback callback) {
  if (g_bubble_)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView();
  views::Button* highlight_button =
      browser_view->toolbar_button_provider()
          ->GetOmniboxPageActionIconContainerView()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall);

  g_bubble_ = new PWAConfirmationBubbleView(anchor_view, highlight_button,
                                            std::move(web_app_info),
                                            std::move(callback));

  views::BubbleDialogDelegateView::CreateBubble(g_bubble_)->Show();
}

}  // namespace chrome
