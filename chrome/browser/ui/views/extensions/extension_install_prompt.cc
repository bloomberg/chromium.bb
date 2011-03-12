// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/layout/layout_constants.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace {

const int kRightColumnWidth = 210;
const int kIconSize = 69;

// Implements the extension installation prompt for Windows.
class InstallDialogContent : public views::View, public views::DialogDelegate {
 public:
  InstallDialogContent(ExtensionInstallUI::Delegate* delegate,
                       const Extension* extension,
                       SkBitmap* icon,
                       ExtensionInstallUI::PromptType type)
        : delegate_(delegate), icon_(NULL), type_(type) {
    // Scale down to icon size, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringFUTF16(ExtensionInstallUI::kHeadingIds[type_],
                                   UTF8ToUTF16(extension->name()))));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);
  }

 private:
  // DialogDelegate
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    switch (button) {
      case MessageBoxFlags::DIALOGBUTTON_OK:
        return UTF16ToWide(
            l10n_util::GetStringUTF16(ExtensionInstallUI::kButtonIds[type_]));
      case MessageBoxFlags::DIALOGBUTTON_CANCEL:
        return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
      default:
        NOTREACHED();
        return L"";
    }
  }

  virtual int GetDefaultDialogButton() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }

  virtual bool Accept() {
    delegate_->InstallUIProceed();
    return true;
  }

  virtual bool Cancel() {
    delegate_->InstallUIAbort();
    return true;
  }

  // WindowDelegate
  virtual bool IsModal() const { return true; }
  virtual std::wstring GetWindowTitle() const {
    return UTF16ToWide(
        l10n_util::GetStringUTF16(ExtensionInstallUI::kTitleIds[type_]));
  }
  virtual views::View* GetContentsView() { return this; }

  // View
  virtual gfx::Size GetPreferredSize() {
    int width = kRightColumnWidth;
    width += kIconSize;
    width += views::kPanelHorizMargin * 3;

    int height = views::kPanelVertMargin * 2;
    height += heading_->GetHeightForWidth(kRightColumnWidth);

    return gfx::Size(width,
                     std::max(height, kIconSize + views::kPanelVertMargin * 2));
  }

  virtual void Layout() {
    int x = views::kPanelHorizMargin;
    int y = views::kPanelVertMargin;

    heading_->SizeToFit(kRightColumnWidth);

    if (heading_->height() <= kIconSize) {
      icon_->SetBounds(x, y, kIconSize, kIconSize);
      x += kIconSize;
      x += views::kPanelHorizMargin;

      heading_->SetX(x);
      heading_->SetY(y + (kIconSize - heading_->height()) / 2);
    } else {
      icon_->SetBounds(x,
                       y + (heading_->height() - kIconSize) / 2,
                       kIconSize,
                       kIconSize);
      x += kIconSize;
      x += views::kPanelHorizMargin;

      heading_->SetX(x);
      heading_->SetY(y);
    }
  }

  ExtensionInstallUI::Delegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;
  ExtensionInstallUI::PromptType type_;

  DISALLOW_COPY_AND_ASSIGN(InstallDialogContent);
};

}  // namespace

void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallUI::Delegate* delegate,
                                const Extension* extension,
                                SkBitmap* icon,
                                ExtensionInstallUI::PromptType type) {
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

  browser::CreateViewsWindow(window->GetNativeHandle(), gfx::Rect(),
      new InstallDialogContent(delegate, extension, icon, type))->Show();
}
