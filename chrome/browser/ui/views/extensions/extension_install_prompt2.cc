// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/layout_constants.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

class Profile;

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// Width of the white permission box. This also is the max width of all
// elements in the right column of the dialog in the case where the extension
// requests permissions.
const int kPermissionBoxWidth = 270;

// Width of the right column of the dialog when the extension requests no
// permissions.
const int kNoPermissionsRightColumnWidth = 210;

// Width of the gray border around the permission box.
const int kPermissionBoxBorderWidth = 1;

// Width of the horizontal padding inside the permission box border.
const int kPermissionBoxHorizontalPadding = 10;

// Width of the vertical padding inside the permission box border.
const int kPermissionBoxVerticalPadding = 11;

// The max width of the individual permission strings inside the permission
// box.
const int kPermissionLabelWidth =
    kPermissionBoxWidth -
    kPermissionBoxBorderWidth * 2 -
    kPermissionBoxHorizontalPadding * 2;

// Heading font size correction.
#if defined(CROS_FONTS_USING_BCI)
const int kHeadingFontSizeDelta = 0;
#else
const int kHeadingFontSizeDelta = 1;
#endif

}  // namespace


// Implements the extension installation prompt for TOOLKIT_VIEWS.
class InstallDialogContent2
    : public views::View, public views::DialogDelegate {
 public:
  InstallDialogContent2(ExtensionInstallUI::Delegate* delegate,
                        const Extension* extension,
                        SkBitmap* icon,
                        const std::vector<string16>& permissions);

 private:
  // DialogDelegate overrides.
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const;
  virtual bool Accept();
  virtual bool Cancel();

  // WindowDelegate overrides.
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView();

  // View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // The delegate that we will call back to when the user accepts or rejects
  // the installation.
  ExtensionInstallUI::Delegate* delegate_;

  // Displays the extension's icon.
  views::ImageView* icon_;

  // Displays the main heading "Install FooBar?".
  views::Label* heading_;

  // Displays the permission box header "The extension will have access to:".
  views::Label* will_have_access_to_;

  // The white box containing the list of permissions the extension requires.
  // This can be NULL if the extension requires no permissions.
  views::View* permission_box_;

  // The labels describing each of the permissions the extension requires.
  std::vector<views::Label*> permissions_;

  // The width of the right column of the dialog. Will be either
  // kPermissionBoxWidth or kNoPermissionsRightColumnWidth, depending on
  // whether the extension requires any permissions.
  int right_column_width_;

  DISALLOW_COPY_AND_ASSIGN(InstallDialogContent2);
};


InstallDialogContent2::InstallDialogContent2(
    ExtensionInstallUI::Delegate* delegate, const Extension* extension,
    SkBitmap* icon, const std::vector<string16>& permissions)
        : delegate_(delegate),
          icon_(NULL),
          heading_(NULL),
          will_have_access_to_(NULL),
          permission_box_(NULL),
          right_column_width_(0) {
  // Scale down to icon size, but allow smaller icons (don't scale up).
  gfx::Size size(icon->width(), icon->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  icon_ = new views::ImageView();
  icon_->SetImageSize(size);
  icon_->SetImage(*icon);
  icon_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_->SetVerticalAlignment(views::ImageView::CENTER);
  AddChildView(icon_);

  heading_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALL_PROMPT_HEADING,
                                 UTF8ToUTF16(extension->name()))));
  heading_->SetFont(heading_->font().DeriveFont(kHeadingFontSizeDelta,
                                                gfx::Font::BOLD));
  heading_->SetMultiLine(true);
  heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(heading_);

  if (permissions.size() == 0) {
    right_column_width_ = kNoPermissionsRightColumnWidth;
  } else {
    right_column_width_ = kPermissionBoxWidth;
    will_have_access_to_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO)));
    will_have_access_to_->SetMultiLine(true);
    will_have_access_to_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(will_have_access_to_);

    permission_box_ = new views::View();
    permission_box_->set_background(
        views::Background::CreateSolidBackground(SK_ColorWHITE));
    permission_box_->set_border(
        views::Border::CreateSolidBorder(kPermissionBoxBorderWidth,
                                         SK_ColorLTGRAY));
    AddChildView(permission_box_);
  }

  for (size_t i = 0; i < permissions.size(); ++i) {
    views::Label* label = new views::Label(UTF16ToWide(permissions[i]));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    permission_box_->AddChildView(label);
    permissions_.push_back(label);
  }
}

std::wstring InstallDialogContent2::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
    case MessageBoxFlags::DIALOGBUTTON_OK:
      return UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_INSTALL_BUTTON));
    case MessageBoxFlags::DIALOGBUTTON_CANCEL:
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
    default:
      NOTREACHED();
      return L"";
  }
}

int InstallDialogContent2::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool InstallDialogContent2::Accept() {
  delegate_->InstallUIProceed();
  return true;
}

bool InstallDialogContent2::Cancel() {
  delegate_->InstallUIAbort();
  return true;
}

bool InstallDialogContent2::IsModal() const {
  return true;
}

std::wstring InstallDialogContent2::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_PROMPT_TITLE));
}

views::View* InstallDialogContent2::GetContentsView() {
  return this;
}

gfx::Size InstallDialogContent2::GetPreferredSize() {
  int width = kPanelHorizMargin * 2;
  width += kIconSize;
  width += kPanelHorizMargin;  // Gutter.
  width += right_column_width_;

  int height = kPanelVertMargin * 2;
  height += heading_->GetHeightForWidth(right_column_width_);

  if (permission_box_) {
    height += kRelatedControlVerticalSpacing;
    height += will_have_access_to_->GetHeightForWidth(right_column_width_);

    height += kRelatedControlVerticalSpacing;
    height += kPermissionBoxBorderWidth * 2;
    height += kPermissionBoxVerticalPadding * 2;

    for (size_t i = 0; i < permissions_.size(); ++i) {
      if (i > 0)
        height += kRelatedControlVerticalSpacing;
      height += permissions_[0]->GetHeightForWidth(kPermissionLabelWidth);
    }
  }

  return gfx::Size(width, std::max(height, kIconSize + kPanelVertMargin * 2));
}

void InstallDialogContent2::Layout() {
  int x = kPanelHorizMargin;
  int y = kPanelVertMargin;

  icon_->SetBounds(x, y, kIconSize, kIconSize);
  x += kIconSize;
  x += kPanelHorizMargin;

  heading_->SizeToFit(right_column_width_);
  heading_->SetX(x);

  // If there's no special permissions, we do a slightly different layout with
  // the heading centered vertically wrt the icon.
  if (!permission_box_) {
    heading_->SetY((GetPreferredSize().height() - heading_->height()) / 2);
    return;
  }

  // Otherwise, do the layout with the permission box.
  heading_->SetY(y);
  y += heading_->height();

  y += kRelatedControlVerticalSpacing;
  will_have_access_to_->SizeToFit(right_column_width_);
  will_have_access_to_->SetX(x);
  will_have_access_to_->SetY(y);
  y += will_have_access_to_->height();

  y += kRelatedControlVerticalSpacing;
  permission_box_->SetX(x);
  permission_box_->SetY(y);

  // First we layout the labels inside the permission box, so that we know how
  // big the box will have to be.
  int label_x = kPermissionBoxBorderWidth + kPermissionBoxHorizontalPadding;
  int label_y = kPermissionBoxBorderWidth + kPermissionBoxVerticalPadding;
  int permission_box_height = kPermissionBoxBorderWidth * 2;
  permission_box_height += kPermissionBoxVerticalPadding * 2;

  for (size_t i = 0; i < permissions_.size(); ++i) {
    if (i > 0) {
      label_y += kRelatedControlVerticalSpacing;
      permission_box_height += kPanelVertMargin;
    }

    permissions_[i]->SizeToFit(kPermissionLabelWidth);
    permissions_[i]->SetX(label_x);
    permissions_[i]->SetY(label_y);

    label_y += permissions_[i]->height();
    permission_box_height += permissions_[i]->height();
  }

  // Now finally we can size the permission box itself.
  permission_box_->SetBounds(permission_box_->x(), permission_box_->y(),
                             right_column_width_, permission_box_height);
}

// static
void ExtensionInstallUI::ShowExtensionInstallUIPrompt2Impl(
    Profile* profile, Delegate* delegate, const Extension* extension,
    SkBitmap* icon,
    const std::vector<string16>& permissions) {
#if defined(OS_CHROMEOS)
  // Use a normal browser window as parent on ChromeOS.
  Browser* browser = BrowserList::FindBrowserWithType(profile,
                                                      Browser::TYPE_NORMAL,
                                                      true);
#else
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
#endif
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
      new InstallDialogContent2(delegate, extension, icon, permissions))
          ->Show();
}
