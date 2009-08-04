// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/standard_layout.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

class Profile;

namespace {

const int kRightColumnWidth = 290;

// Implements the extension installation prompt for Windows.
// TODO(aa): It would be cool to add an "extensions threat level" when we have
// granular permissions implemented:
// - red: npapi
// - orange: access to any domains
// - yellow: access to browser data
// - green: nothing
// We could have a collection of funny descriptions for each color.
class InstallDialogContent : public views::View, public views::DialogDelegate {
 public:
  InstallDialogContent(CrxInstaller* crx_installer, Extension* extension,
                       SkBitmap* icon)
      : crx_installer_(crx_installer), icon_(NULL) {
    if (icon) {
      icon_ = new views::ImageView();
      icon_->SetImage(*icon);
      AddChildView(icon_);
    }

    heading_ = new views::Label(
        l10n_util::GetStringF(IDS_EXTENSION_PROMPT_HEADING,
                              UTF8ToWide(extension->name())));
    heading_->SetFont(heading_->GetFont().DeriveFont(1, gfx::Font::BOLD));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    // Pick a random warning.
    std::wstring warnings[] = {
      l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_1),
      l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_2),
      l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_3)
    };
    warning_ = new views::Label(
        warnings[base::RandInt(0, arraysize(warnings) - 1)]);
    warning_->SetMultiLine(true);
    warning_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(warning_);

    severe_ = new views::Label(
        l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_SEVERE));
    severe_->SetMultiLine(true);
    severe_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    severe_->SetFont(heading_->GetFont().DeriveFont(0, gfx::Font::BOLD));
    severe_->SetColor(SK_ColorRED);
    AddChildView(severe_);
  }

 private:
  // DialogDelegate
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    switch (button) {
      case MessageBoxFlags::DIALOGBUTTON_OK:
        return l10n_util::GetString(IDS_EXTENSION_PROMPT_INSTALL_BUTTON);
      case MessageBoxFlags::DIALOGBUTTON_CANCEL:
        return l10n_util::GetString(IDS_EXTENSION_PROMPT_CANCEL_BUTTON);
      default:
        NOTREACHED();
        return L"";
    }
  }

  virtual int GetDefaultDialogButton() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }

  virtual bool Accept() {
    crx_installer_->ContinueInstall();
    return true;
  }

  virtual bool Cancel() {
    crx_installer_->AbortInstall();
    return true;
  }


  // WindowDelegate
  virtual bool IsModal() const { return true; }
  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_TITLE);
  }
  virtual views::View* GetContentsView() { return this; }

  // View
  virtual gfx::Size GetPreferredSize() {
    int width = kRightColumnWidth + kPanelHorizMargin + kPanelHorizMargin;

    if (icon_) {
      width += Extension::EXTENSION_ICON_LARGE;
      width += kPanelHorizMargin;
    }

    int height = kPanelVertMargin * 2;
    height += heading_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;
    height += warning_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;
    height += severe_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;

    return gfx::Size(width, std::max(height,
        static_cast<int>(Extension::EXTENSION_ICON_LARGE)));
  }

  virtual void Layout() {
    int x = kPanelHorizMargin;
    int y = kPanelVertMargin;

    if (icon_) {
      icon_->SetBounds(x, y, Extension::EXTENSION_ICON_LARGE,
                       Extension::EXTENSION_ICON_LARGE);
      x += Extension::EXTENSION_ICON_LARGE;
      x += kPanelHorizMargin;
    }

    heading_->SizeToFit(kRightColumnWidth);
    heading_->SetX(x);
    heading_->SetY(y);
    y += heading_->height();

    y += kPanelVertMargin;

    warning_->SizeToFit(kRightColumnWidth);
    warning_->SetX(x);
    warning_->SetY(y);
    y += warning_->height();

    y += kPanelVertMargin;

    severe_->SizeToFit(kRightColumnWidth);
    severe_->SetX(x);
    severe_->SetY(y);
    y += severe_->height();
  }

  scoped_refptr<CrxInstaller> crx_installer_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* warning_;
  views::Label* severe_;

  DISALLOW_COPY_AND_ASSIGN(InstallDialogContent);
};

} // namespace

void ExtensionInstallUI::ShowExtensionInstallPrompt(Profile* profile,
                                                    CrxInstaller* crx_installer,
                                                    Extension* extension,
                                                    SkBitmap* icon) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    crx_installer->AbortInstall();
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    crx_installer->AbortInstall();
    return;
  }

  views::Window::CreateChromeWindow(window->GetNativeHandle(), gfx::Rect(),
      new InstallDialogContent(crx_installer, extension, icon))->Show();
}
