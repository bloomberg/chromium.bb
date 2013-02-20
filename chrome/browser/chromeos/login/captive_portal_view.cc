// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captive_portal_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kCaptivePortalStartURL[] = "http://clients3.google.com/generate_204";

}  // namespace

namespace chromeos {

CaptivePortalView::CaptivePortalView(Profile* profile,
                                     CaptivePortalWindowProxy* proxy)
    : SimpleWebViewDialog(profile),
      proxy_(proxy),
      redirected_(false) {
}

CaptivePortalView::~CaptivePortalView() {
}

void CaptivePortalView::StartLoad() {
  SimpleWebViewDialog::StartLoad(GURL(kCaptivePortalStartURL));
}

bool CaptivePortalView::CanResize() const {
  return false;
}

ui::ModalType CaptivePortalView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 CaptivePortalView::GetWindowTitle() const {
  string16 network_name;
  const Network* active_network =
      CrosLibrary::Get()->GetNetworkLibrary()->active_network();
  if (active_network) {
    network_name = ASCIIToUTF16(active_network->name());
  } else {
    DLOG(ERROR) << "No active network, but captive portal window is shown.";
  }

  return l10n_util::GetStringFUTF16(IDS_LOGIN_CAPTIVE_PORTAL_WINDOW_TITLE,
                                    network_name);
}

bool CaptivePortalView::ShouldShowWindowTitle() const {
  return true;
}

void CaptivePortalView::NavigationStateChanged(
    const content::WebContents* source, unsigned changed_flags) {
  SimpleWebViewDialog::NavigationStateChanged(source, changed_flags);

  // Naive way to determine the redirection. This won't be needed after portal
  // detection will be done on the Chrome side.
  GURL url = source->GetURL();
  // Note, |url| will be empty for "client3.google.com/generate_204" page.
  if (!redirected_  && url != GURL::EmptyGURL() &&
      url != GURL(kCaptivePortalStartURL)) {
    DLOG(INFO) << kCaptivePortalStartURL << " vs " << url.spec();
    redirected_ = true;
    proxy_->OnRedirected();
  }
}

void CaptivePortalView::LoadingStateChanged(content::WebContents* source) {
  SimpleWebViewDialog::LoadingStateChanged(source);
  // TODO(nkostylev): Fix case of no connectivity, check HTTP code returned.
  // Disable this heuristic as it has false positives.
  // Relying on just shill portal check to close dialog is fine.
  // if (!is_loading && !redirected_)
  //   proxy_->OnOriginalURLLoaded();
}

}  // namespace chromeos
