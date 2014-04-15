// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace attestation {

namespace {

const int kDialogMaxWidthInPixel = 400;

}  // namespace

// static
void PlatformVerificationDialog::ShowDialog(
    content::WebContents* web_contents,
    const PlatformVerificationFlow::Delegate::ConsentCallback& callback) {
  GURL url = web_contents->GetLastCommittedURL();
  // In the case of an extension or hosted app, the origin of the request is
  // best described by the extension / app name.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())->
          enabled_extensions().GetExtensionOrAppByURL(url);
  std::string origin = extension ? extension->name() : url.GetOrigin().spec();

  PlatformVerificationDialog* dialog = new PlatformVerificationDialog(
      chrome::FindBrowserWithWebContents(web_contents),
      base::UTF8ToUTF16(origin),
      callback);

  // Sets up the dialog widget and shows it.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  web_modal::WebContentsModalDialogManagerDelegate* modal_delegate =
      web_contents_modal_dialog_manager->delegate();
  views::Widget* widget = views::Widget::CreateWindowAsFramelessChild(
      dialog, modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowModalDialog(
      widget->GetNativeView());
  widget->Show();
}

PlatformVerificationDialog::~PlatformVerificationDialog() {
}

PlatformVerificationDialog::PlatformVerificationDialog(
    Browser* browser,
    const base::string16& domain,
    const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
    : browser_(browser),
      domain_(domain),
      callback_(callback) {
  SetLayoutManager(new views::FillLayout());
  SetBorder(views::Border::CreateEmptyBorder(
      0, views::kButtonHEdgeMarginNew, 0, views::kButtonHEdgeMarginNew));
  const base::string16 learn_more = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  std::vector<size_t> offsets;
  base::string16 headline = l10n_util::GetStringFUTF16(
      IDS_PLATFORM_VERIFICATION_DIALOG_HEADLINE, domain_, learn_more, &offsets);
  views::StyledLabel* headline_label = new views::StyledLabel(headline, this);
  headline_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + learn_more.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  AddChildView(headline_label);
}

bool PlatformVerificationDialog::Cancel() {
  callback_.Run(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  return true;
}

bool PlatformVerificationDialog::Accept() {
  callback_.Run(PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW);
  return true;
}

bool PlatformVerificationDialog::Close() {
  // This method is called when the tab is closed and in that case the decision
  // hasn't been made yet.
  callback_.Run(PlatformVerificationFlow::CONSENT_RESPONSE_NONE);
  return true;
}

base::string16 PlatformVerificationDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_PLATFORM_VERIFICATION_DIALOG_ALLOW);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringFUTF16(
          IDS_PLATFORM_VERIFICATION_DIALOG_DENY, domain_);
    default:
      NOTREACHED();
  }
  return base::string16();
}

ui::ModalType PlatformVerificationDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size PlatformVerificationDialog::GetPreferredSize() {
  return gfx::Size(kDialogMaxWidthInPixel,
                   GetHeightForWidth(kDialogMaxWidthInPixel));
}

void PlatformVerificationDialog::StyledLabelLinkClicked(const gfx::Range& range,
                                                        int event_flags) {
  chrome::ShowSingletonTab(browser_, GURL(
      chrome::kEnhancedPlaybackNotificationLearnMoreURL));
}

}  // namespace attestation
}  // namespace chromeos
