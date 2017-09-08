// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {
namespace attestation {

namespace {

const int kDialogMaxWidthInPixel = 400;

}  // namespace

// static
views::Widget* PlatformVerificationDialog::ShowDialog(
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    const ConsentCallback& callback) {
  // This could happen when the permission is requested from an extension. See
  // http://crbug.com/728534
  // TODO(wittman): Remove this check after ShowWebModalDialogViews() API is
  // improved/fixed. See http://crbug.com/733355
  if (!web_modal::WebContentsModalDialogManager::FromWebContents(
          web_contents)) {
    DVLOG(1) << "WebContentsModalDialogManager not registered for WebContents.";
    return nullptr;
  }

  // In the case of an extension or hosted app, the origin of the request is
  // best described by the extension / app name.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->enabled_extensions()
          .GetExtensionOrAppByURL(web_contents->GetLastCommittedURL());

  // TODO(xhwang): We should only show the name if the request if from the
  // extension's true frame. See http://crbug.com/455821
  std::string origin = extension ? extension->name() : requesting_origin.spec();

  PlatformVerificationDialog* dialog = new PlatformVerificationDialog(
      web_contents,
      base::UTF8ToUTF16(origin),
      callback);

  return constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

PlatformVerificationDialog::~PlatformVerificationDialog() {
}

PlatformVerificationDialog::PlatformVerificationDialog(
    content::WebContents* web_contents,
    const base::string16& domain,
    const ConsentCallback& callback)
    : content::WebContentsObserver(web_contents),
      domain_(domain),
      callback_(callback) {
  SetLayoutManager(new views::FillLayout());

  gfx::Insets dialog_insets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(0, dialog_insets.left(), 0,
                                     dialog_insets.right()));
  const base::string16 learn_more = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  std::vector<size_t> offsets;
  base::string16 headline = l10n_util::GetStringFUTF16(
      IDS_PLATFORM_VERIFICATION_DIALOG_HEADLINE, domain_, learn_more, &offsets);
  views::StyledLabel* headline_label = new views::StyledLabel(headline, this);
  headline_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + learn_more.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  AddChildView(headline_label);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PLATFORM_VERIFICATION);
}

bool PlatformVerificationDialog::Cancel() {
  // This method is called when user clicked "Block" button.
  callback_.Run(CONSENT_RESPONSE_DENY);
  return true;
}

bool PlatformVerificationDialog::Accept() {
  // This method is called when user clicked "Allow" button.
  callback_.Run(CONSENT_RESPONSE_ALLOW);
  return true;
}

bool PlatformVerificationDialog::Close() {
  // This method is called when user clicked "x" or pressed "Esc" to dismiss the
  // dialog, the permission request is canceled, or when the tab containing this
  // dialog is closed.
  callback_.Run(CONSENT_RESPONSE_NONE);
  return true;
}

base::string16 PlatformVerificationDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
    default:
      NOTREACHED();
  }
  return base::string16();
}

ui::ModalType PlatformVerificationDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size PlatformVerificationDialog::CalculatePreferredSize() const {
  return gfx::Size(kDialogMaxWidthInPixel,
                   GetHeightForWidth(kDialogMaxWidthInPixel));
}

void PlatformVerificationDialog::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  const GURL learn_more_url(chrome::kEnhancedPlaybackNotificationLearnMoreURL);

  // |web_contents()| might not be in a browser in case of v2 apps. In that
  // case, open a new tab in the usual way.
  if (!browser) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    chrome::NavigateParams params(
        profile, learn_more_url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::SINGLETON_TAB;
    chrome::Navigate(&params);
  } else {
    chrome::ShowSingletonTab(browser, learn_more_url);
  }
}

void PlatformVerificationDialog::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  views::Widget* widget = GetWidget();
  if (widget)
    widget->Close();
}

}  // namespace attestation
}  // namespace chromeos
