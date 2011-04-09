// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
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

class ExtensionUninstallDialogView : public views::View,
                                     public views::DialogDelegate {
 public:
  ExtensionUninstallDialogView(ExtensionUninstallDialog::Delegate* delegate,
                               const Extension* extension,
                               SkBitmap* icon)
        : delegate_(delegate),
          icon_(NULL) {
    // Scale down to icon size, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_HEADING,
                                   UTF8ToUTF16(extension->name()))));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);
  }

 private:
  // views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE {
    switch (button) {
      case MessageBoxFlags::DIALOGBUTTON_OK:
        return UTF16ToWide(
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
      case MessageBoxFlags::DIALOGBUTTON_CANCEL:
        return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
      default:
        NOTREACHED();
        return L"";
    }
  }

  virtual int GetDefaultDialogButton() const OVERRIDE {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }

  virtual bool Accept() OVERRIDE {
    delegate_->ExtensionDialogAccepted();
    return true;
  }

  virtual bool Cancel() OVERRIDE {
    delegate_->ExtensionDialogCanceled();
    return true;
  }

  // views::WindowDelegate:
  virtual bool IsModal() const OVERRIDE { return true; }
  virtual std::wstring GetWindowTitle() const OVERRIDE {
    return UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE));
  }
  virtual views::View* GetContentsView() { return this; }

  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    int width = kRightColumnWidth;
    width += kIconSize;
    width += views::kPanelHorizMargin * 3;

    int height = views::kPanelVertMargin * 2;
    height += heading_->GetHeightForWidth(kRightColumnWidth);

    return gfx::Size(width,
                     std::max(height, kIconSize + views::kPanelVertMargin * 2));
  }

  virtual void Layout() OVERRIDE {
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

  ExtensionUninstallDialog::Delegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogView);
};

}  // namespace

// static
void ExtensionUninstallDialog::Show(
    Profile* profile,
    ExtensionUninstallDialog::Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->ExtensionDialogCanceled();
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    delegate->ExtensionDialogCanceled();
    return;
  }

  browser::CreateViewsWindow(window->GetNativeHandle(), gfx::Rect(),
      new ExtensionUninstallDialogView(delegate, extension, icon))->Show();
}
