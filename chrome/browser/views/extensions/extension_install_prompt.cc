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

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

class Profile;

namespace {

const int kRightColumnWidth = 270;
const int kIconSize = 85;

// Implements the extension installation prompt for Windows.
class InstallDialogContent : public views::View, public views::DialogDelegate {
 public:
  InstallDialogContent(ExtensionInstallUI::Delegate* delegate,
      Extension* extension, SkBitmap* icon, const std::wstring& warning_text)
          : delegate_(delegate), icon_(NULL) {
    icon_ = new views::ImageView();
    icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(
        l10n_util::GetStringF(IDS_EXTENSION_PROMPT_HEADING,
                              UTF8ToWide(extension->name())));
    heading_->SetFont(heading_->GetFont().DeriveFont(1, gfx::Font::BOLD));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    warning_ = new views::Label(warning_text);
    warning_->SetMultiLine(true);
    warning_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(warning_);
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
    delegate_->ContinueInstall();
    return true;
  }

  virtual bool Cancel() {
    delegate_->AbortInstall();
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
    width += kIconSize;
    width += kPanelHorizMargin;

    int height = kPanelVertMargin * 2;
    height += heading_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;
    height += warning_->GetHeightForWidth(kRightColumnWidth);
    height += kPanelVertMargin;

    return gfx::Size(width, std::max(height, kIconSize + kPanelVertMargin * 2));
  }

  virtual void Layout() {
    int x = kPanelHorizMargin;
    int y = kPanelVertMargin;

    icon_->SetBounds(x, y, kIconSize, kIconSize);
    x += kIconSize;
    x += kPanelHorizMargin;

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
  }

  ExtensionInstallUI::Delegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* warning_;

  DISALLOW_COPY_AND_ASSIGN(InstallDialogContent);
};

}  // namespace

void ExtensionInstallUI::ShowExtensionInstallPrompt(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const std::wstring& warning_text) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->ContinueInstall();
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    delegate->AbortInstall();
    return;
  }

  views::Window::CreateChromeWindow(window->GetNativeHandle(), gfx::Rect(),
      new InstallDialogContent(delegate, extension, icon,
                               warning_text))->Show();
}

void ExtensionInstallUI::ShowExtensionInstallError(const std::string& error) {
#if defined(OS_WIN)
  win_util::MessageBox(NULL, UTF8ToWide(error),
      l10n_util::GetString(IDS_EXTENSION_INSTALL_FAILURE_TITLE),
      MB_OK | MB_SETFOREGROUND);
#else
  // TODO(port): Port this over to OS_*
  NOTREACHED();
#endif
}
