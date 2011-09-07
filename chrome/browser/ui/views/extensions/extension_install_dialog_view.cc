// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/border.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/link_listener.h"
#include "views/controls/separator.h"
#include "views/layout/box_layout.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/view.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// Width of the left column of the dialog when the extension requests
// permissions.
const int kPermissionsLeftColumnWidth = 250;

// Width of the left column of the dialog when the extension requests no
// permissions.
const int kNoPermissionsLeftColumnWidth = 200;

// Heading font size correction.
#if defined(CROS_FONTS_USING_BCI)
const int kHeadingFontSizeDelta = 0;
#else
const int kHeadingFontSizeDelta = 1;
#endif

const int kRatingFontSizeDelta = -1;

void AddResourceIcon(const SkBitmap* skia_image, void* data) {
  views::View* parent = static_cast<views::View*>(data);
  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(*skia_image);
  parent->AddChildView(image_view);
}

}  // namespace

// Implements the extension installation dialog for TOOLKIT_VIEWS.
class ExtensionInstallDialogView : public views::DialogDelegateView,
                                   public views::LinkListener {
 public:
  ExtensionInstallDialogView(ExtensionInstallUI::Delegate* delegate,
                             const Extension* extension,
                             SkBitmap* skia_icon,
                             const ExtensionInstallUI::Prompt& prompt);
  virtual ~ExtensionInstallDialogView();

 private:
  // views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WindowDelegate:
  virtual bool IsModal() const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  bool is_inline_install() {
    return prompt_.type() == ExtensionInstallUI::INLINE_INSTALL_PROMPT;
  }

  ExtensionInstallUI::Delegate* delegate_;
  const Extension* extension_;
  ExtensionInstallUI::Prompt prompt_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

ExtensionInstallDialogView::ExtensionInstallDialogView(
    ExtensionInstallUI::Delegate* delegate,
    const Extension* extension,
    SkBitmap* skia_icon,
    const ExtensionInstallUI::Prompt& prompt)
    : delegate_(delegate),
      extension_(extension),
      prompt_(prompt) {
  // Possible grid layouts:
  // Inline install
  //      w/ permissions                 no permissions
  // +--------------------+------+  +--------------+------+
  // | heading            | icon |  | heading      | icon |
  // +--------------------|      |  +--------------|      |
  // | rating             |      |  | rating       |      |
  // +--------------------|      |  +--------------+      |
  // | user_count         |      |  | user_count   |      |
  // +--------------------|      |  +--------------|      |
  // | store_link         |      |  | store_link   |      |
  // +--------------------+------+  +--------------+------+
  // |      separator            |
  // +--------------------+------+
  // | permissions_header |      |
  // +--------------------+------+
  // | permission1        |      |
  // +--------------------+------+
  // | permission2        |      |
  // +--------------------+------+
  //
  // Regular install
  //      w/ permissions                 no permissions
  // +--------------------+------+  +--------------+------+
  // | heading            | icon |  | heading      | icon |
  // +--------------------|      |  +--------------+------+
  // | permissions_header |      |
  // +--------------------|      |
  // | permission1        |      |
  // +--------------------|      |
  // | permission2        |      |
  // +--------------------+------+

  using views::GridLayout;
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  int left_column_width = prompt.GetPermissionCount() > 0 ?
      kPermissionsLeftColumnWidth : kNoPermissionsLeftColumnWidth;

  column_set->AddColumn(GridLayout::LEADING,
                        GridLayout::FILL,
                        0, // no resizing
                        GridLayout::USE_PREF,
                        0, // no fixed with
                        left_column_width);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  column_set->AddColumn(GridLayout::LEADING,
                        GridLayout::LEADING,
                        0, // no resizing
                        GridLayout::USE_PREF,
                        0, // no fixed width
                        kIconSize);

  layout->StartRow(0, column_set_id);

  views::Label* heading = new views::Label(UTF16ToWide(
      prompt.GetHeading(extension->name())));
  heading->SetFont(heading->font().DeriveFont(kHeadingFontSizeDelta,
                                              gfx::Font::BOLD));
  heading->SetMultiLine(true);
  heading->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  heading->SizeToFit(left_column_width);
  layout->AddView(heading);

  // Scale down to icon size, but allow smaller icons (don't scale up).
  gfx::Size size(skia_icon->width(), skia_icon->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  views::ImageView* icon = new views::ImageView();
  icon->SetImageSize(size);
  icon->SetImage(*skia_icon);
  icon->SetHorizontalAlignment(views::ImageView::CENTER);
  icon->SetVerticalAlignment(views::ImageView::CENTER);
  int icon_row_span = 1;
  if (is_inline_install()) {
    // Also span the rating, user_count and store_link rows.
    icon_row_span = 4;
  } else if (prompt.GetPermissionCount()) {
    // Also span the permission header and each of the permission rows (all have
    // a padding row above it).
    icon_row_span = 3 + prompt.GetPermissionCount() * 2;
  }
  layout->AddView(icon, 1, icon_row_span);

  if (is_inline_install()) {
    layout->StartRow(0, column_set_id);
    views::View* rating = new views::View();
    rating->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    layout->AddView(rating);
    prompt.AppendRatingStars(AddResourceIcon, rating);

    views::Label* rating_count = new views::Label(
        UTF16ToWide(prompt.GetRatingCount()));
    rating_count->SetFont(
        rating_count->font().DeriveFont(kRatingFontSizeDelta));
    // Add some space between the stars and the rating count.
    rating_count->set_border(views::Border::CreateEmptyBorder(0, 2, 0, 0));
    rating->AddChildView(rating_count);

    layout->StartRow(0, column_set_id);
    views::Label* user_count = new views::Label(
        UTF16ToWide(prompt.GetUserCount()));
    user_count->SetColor(SK_ColorGRAY);
    user_count->SetFont(user_count->font().DeriveFont(kRatingFontSizeDelta));
    layout->AddView(user_count);

    layout->StartRow(0, column_set_id);
    views::Link* store_link = new views::Link(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_STORE_LINK)));
    store_link->SetFont(store_link->font().DeriveFont(kRatingFontSizeDelta));
    store_link->set_listener(this);
    layout->AddView(store_link);
  }

  if (prompt.GetPermissionCount()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    if (is_inline_install()) {
      layout->StartRow(0, column_set_id);
      views::Separator* separator = new views::Separator();
      layout->AddView(separator, 3, 1, GridLayout::FILL, GridLayout::FILL);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }

    layout->StartRow(0, column_set_id);
    views::Label* permissions_header = new views::Label(UTF16ToWide(
        prompt.GetPermissionsHeader()));
    permissions_header->SetMultiLine(true);
    permissions_header->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    permissions_header->SizeToFit(left_column_width);
    layout->AddView(permissions_header);

    for (size_t i = 0; i < prompt.GetPermissionCount(); ++i) {
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, column_set_id);
      views::Label* permission_label = new views::Label(
          UTF16ToWide(prompt.GetPermission(i)));
      permission_label->SetMultiLine(true);
      permission_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      permission_label->SizeToFit(left_column_width);
      layout->AddView(permission_label);
    }
  }
}

ExtensionInstallDialogView::~ExtensionInstallDialogView() {
}

std::wstring ExtensionInstallDialogView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
    case MessageBoxFlags::DIALOGBUTTON_OK:
      return UTF16ToWide(prompt_.GetAcceptButtonLabel());
    case MessageBoxFlags::DIALOGBUTTON_CANCEL:
      return prompt_.HasAbortButtonLabel() ?
          UTF16ToWide(prompt_.GetAbortButtonLabel()) :
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
    default:
      NOTREACHED();
      return std::wstring();
  }
}

int ExtensionInstallDialogView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool ExtensionInstallDialogView::Cancel() {
  delegate_->InstallUIAbort(true);
  return true;
}

bool ExtensionInstallDialogView::Accept() {
  delegate_->InstallUIProceed();
  return true;
}

bool ExtensionInstallDialogView::IsModal() const {
  return true;
}

std::wstring ExtensionInstallDialogView::GetWindowTitle() const {
  return UTF16ToWide(prompt_.GetDialogTitle());
}

views::View* ExtensionInstallDialogView::GetContentsView() {
  return this;
}

void ExtensionInstallDialogView::LinkClicked(views::Link* source,
                                             int event_flags) {
  GURL store_url(
      extension_urls::GetWebstoreItemDetailURLPrefix() + extension_->id());
  BrowserList::GetLastActive()->
      OpenURL(store_url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  GetWidget()->Close();
}

void ShowExtensionInstallDialog(
    Profile* profile,
    ExtensionInstallUI::Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallUI::Prompt& prompt) {
#if defined(OS_CHROMEOS)
  // Use a tabbed browser window as parent on ChromeOS.
  Browser* browser = BrowserList::FindTabbedBrowser(profile, true);
#else
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
#endif
  if (!browser) {
    delegate->InstallUIAbort(false);
    return;
  }

  BrowserWindow* browser_window = browser->window();
  if (!browser_window) {
    delegate->InstallUIAbort(false);
    return;
  }

  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      delegate, extension, icon, prompt);

  views::Widget* window =  browser::CreateViewsWindow(
      browser_window->GetNativeHandle(), dialog);

  window->Show();
}
