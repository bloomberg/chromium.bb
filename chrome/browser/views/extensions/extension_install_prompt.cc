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
#include "views/controls/button/checkbox.h"
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

// Since apps don't (currently) have any privilege disclosure text, the dialog
// looks a bit empty if it is sized the same as extensions. So we scale
// everything down a bit for apps for the time being.
const int kRightColumnWidthApp = 210;
const int kRightColumnWidthExtension = 270;
const int kIconSizeApp = 69;
const int kIconSizeExtension = 85;

// Implements the extension installation prompt for Windows.
class InstallDialogContent : public views::View, public views::DialogDelegate {
 public:
  InstallDialogContent(ExtensionInstallUI::Delegate* delegate,
      Extension* extension, SkBitmap* icon, const std::wstring& warning_text,
      bool is_uninstall)
          : delegate_(delegate), icon_(NULL), warning_(NULL),
            create_shortcut_(NULL), is_uninstall_(is_uninstall) {
    if (extension->IsApp()) {
      icon_size_ = kIconSizeApp;
      right_column_width_ = kRightColumnWidthApp;
    } else {
      icon_size_ = kIconSizeExtension;
      right_column_width_ = kRightColumnWidthExtension;
    }

    // Scale down to icon size, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > icon_size_ || size.height() > icon_size_)
      size = gfx::Size(icon_size_, icon_size_);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(
        l10n_util::GetStringF(is_uninstall ?
                                  IDS_EXTENSION_UNINSTALL_PROMPT_HEADING :
                                  IDS_EXTENSION_INSTALL_PROMPT_HEADING,
                              UTF8ToWide(extension->name())));
    heading_->SetFont(heading_->GetFont().DeriveFont(1, gfx::Font::BOLD));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    if (!is_uninstall && extension->IsApp()) {
      create_shortcut_ = new views::Checkbox(
          l10n_util::GetString(IDS_EXTENSION_PROMPT_CREATE_SHORTCUT));
      create_shortcut_->SetChecked(true);
      create_shortcut_->SetMultiLine(true);
      AddChildView(create_shortcut_);
    } else {
      warning_ = new views::Label(warning_text);
      warning_->SetMultiLine(true);
      warning_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(warning_);
    }
  }

 private:
  // DialogDelegate
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    switch (button) {
      case MessageBoxFlags::DIALOGBUTTON_OK:
        if (is_uninstall_)
          return l10n_util::GetString(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON);
        else
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
    delegate_->InstallUIProceed(
        create_shortcut_ && create_shortcut_->checked());
    return true;
  }

  virtual bool Cancel() {
    delegate_->InstallUIAbort();
    return true;
  }


  // WindowDelegate
  virtual bool IsModal() const { return true; }
  virtual std::wstring GetWindowTitle() const {
    if (is_uninstall_)
      return l10n_util::GetString(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
    else
      return l10n_util::GetString(IDS_EXTENSION_INSTALL_PROMPT_TITLE);
  }
  virtual views::View* GetContentsView() { return this; }

  // View
  virtual gfx::Size GetPreferredSize() {
    int width = right_column_width_ + kPanelHorizMargin + kPanelHorizMargin;
    width += icon_size_;
    width += kPanelHorizMargin;

    int height = kPanelVertMargin * 2;
    height += heading_->GetHeightForWidth(right_column_width_);
    height += kPanelVertMargin;

    if (warning_)
      height += warning_->GetHeightForWidth(right_column_width_);
    else
      height += create_shortcut_->GetPreferredSize().height();

    height += kPanelVertMargin;

    return gfx::Size(width, std::max(height, icon_size_ + kPanelVertMargin * 2));
  }

  virtual void Layout() {
    int x = kPanelHorizMargin;
    int y = kPanelVertMargin;

    icon_->SetBounds(x, y, icon_size_, icon_size_);
    x += icon_size_;
    x += kPanelHorizMargin;

    heading_->SizeToFit(right_column_width_);
    heading_->SetX(x);
    heading_->SetY(y);
    y += heading_->height();

    y += kPanelVertMargin;

    if (create_shortcut_) {
      create_shortcut_->SetBounds(x, y, right_column_width_, 0);
      create_shortcut_->SetBounds(x, y, right_column_width_,
          create_shortcut_->GetPreferredSize().height());

      int bottom_aligned = icon_->y() + icon_->height() -
          create_shortcut_->height();
      if (bottom_aligned > y) {
        create_shortcut_->SetY(bottom_aligned);
        y = bottom_aligned;
      }
      y += create_shortcut_->height();
    } else {
      warning_->SizeToFit(right_column_width_);
      warning_->SetX(x);
      warning_->SetY(y);
      y += warning_->height();
    }

    y += kPanelVertMargin;
  }

  ExtensionInstallUI::Delegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* warning_;
  views::Checkbox* create_shortcut_;
  bool is_uninstall_;
  int right_column_width_;
  int icon_size_;

  DISALLOW_COPY_AND_ASSIGN(InstallDialogContent);
};

}  // namespace

// static
void ExtensionInstallUI::ShowExtensionInstallUIPromptImpl(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const string16& warning_text, bool is_uninstall) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->InstallUIAbort();
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    delegate->InstallUIAbort();
    return;
  }

  views::Window::CreateChromeWindow(window->GetNativeHandle(), gfx::Rect(),
      new InstallDialogContent(delegate, extension, icon,
                               UTF16ToWideHack(warning_text),
                               is_uninstall))->Show();
}
