// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

namespace {

const int kRightColumnWidth = 210;
const int kIconSize = 69;

class ExtensionUninstallDialogDelegateView;

// Returns parent window for extension uninstall dialog.
// For ash, use app list window if it is visible.
// For other platforms or when app list is not visible on ash,
// use browser window of given |profile|.
// Note this function could return NULL if ash app list is not visible and
// there is no browser window open for |profile|.
gfx::NativeWindow GetParent(Profile* profile) {
#if defined(USE_ASH)
  gfx::NativeWindow app_list = ash::Shell::GetInstance()->GetAppListWindow();
  if (app_list)
    return app_list;
#endif

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (browser && browser->window())
    return browser->window()->GetNativeHandle();

  return NULL;
}

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews : public ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(Profile* profile,
                                ExtensionUninstallDialog::Delegate* delegate);
  virtual ~ExtensionUninstallDialogViews();

  // Forwards the accept and cancels to the delegate.
  void ExtensionUninstallAccepted();
  void ExtensionUninstallCanceled();

  ExtensionUninstallDialogDelegateView* view() { return view_; }

 private:
  void Show() OVERRIDE;

  ExtensionUninstallDialogDelegateView* view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogViews);
};

// The dialog's view, owned by the views framework.
class ExtensionUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  ExtensionUninstallDialogDelegateView(
      ExtensionUninstallDialogViews* dialog_view,
      const Extension* extension,
      SkBitmap* icon);
  virtual ~ExtensionUninstallDialogDelegateView();

  // Called when the ExtensionUninstallDialog has been destroyed to make sure
  // we invalidate pointers.
  void DialogDestroyed() { dialog_ = NULL; }

 private:
  // views::DialogDelegate:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE {
    return ui::DIALOG_BUTTON_CANCEL;
  }
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_WINDOW;
  }
  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  virtual void Layout() OVERRIDE;

  ExtensionUninstallDialogViews* dialog_;

  views::ImageView* icon_;
  views::Label* heading_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogDelegateView);
};

ExtensionUninstallDialogViews::ExtensionUninstallDialogViews(
    Profile* profile, ExtensionUninstallDialog::Delegate* delegate)
    : ExtensionUninstallDialog(profile, delegate),
      view_(NULL) {
}

ExtensionUninstallDialogViews::~ExtensionUninstallDialogViews() {
  // Close the widget (the views framework will delete view_).
  if (view_) {
    view_->DialogDestroyed();
    view_->GetWidget()->CloseNow();
  }
}

void ExtensionUninstallDialogViews::Show() {
  gfx::NativeWindow parent = GetParent(profile_);
  if (!parent) {
    delegate_->ExtensionUninstallCanceled();
    return;
  }

  view_ = new ExtensionUninstallDialogDelegateView(this, extension_, &icon_);
  views::Widget::CreateWindowWithParent(view_, parent)->Show();
}

void ExtensionUninstallDialogViews::ExtensionUninstallAccepted() {
  // The widget gets destroyed when the dialog is accepted.
  view_ = NULL;
  delegate_->ExtensionUninstallAccepted();
}

void ExtensionUninstallDialogViews::ExtensionUninstallCanceled() {
  // The widget gets destroyed when the dialog is canceled.
  view_ = NULL;
  delegate_->ExtensionUninstallCanceled();
}

ExtensionUninstallDialogDelegateView::ExtensionUninstallDialogDelegateView(
    ExtensionUninstallDialogViews* dialog_view,
    const Extension* extension,
    SkBitmap* icon)
    : dialog_(dialog_view) {
  // Scale down to icon size, but allow smaller icons (don't scale up).
  gfx::Size size(icon->width(), icon->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  icon_ = new views::ImageView();
  icon_->SetImageSize(size);
  icon_->SetImage(*icon);
  AddChildView(icon_);

  heading_ = new views::Label(
      l10n_util::GetStringFUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_HEADING,
                                 UTF8ToUTF16(extension->name())));
  heading_->SetMultiLine(true);
  AddChildView(heading_);
}

ExtensionUninstallDialogDelegateView::~ExtensionUninstallDialogDelegateView() {
}

string16 ExtensionUninstallDialogDelegateView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON : IDS_CANCEL);
}

bool ExtensionUninstallDialogDelegateView::Accept() {
  if (dialog_)
    dialog_->ExtensionUninstallAccepted();
  return true;
}

bool ExtensionUninstallDialogDelegateView::Cancel() {
  if (dialog_)
    dialog_->ExtensionUninstallCanceled();
  return true;
}

string16 ExtensionUninstallDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
}


gfx::Size ExtensionUninstallDialogDelegateView::GetPreferredSize() {
  int width = kRightColumnWidth;
  width += kIconSize;
  width += views::kPanelHorizMargin * 3;

  int height = views::kPanelVertMargin * 2;
  height += heading_->GetHeightForWidth(kRightColumnWidth);

  return gfx::Size(width,
                   std::max(height, kIconSize + views::kPanelVertMargin * 2));
}

void ExtensionUninstallDialogDelegateView::Layout() {
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

}  // namespace

// static
ExtensionUninstallDialog* ExtensionUninstallDialog::Create(
    Profile* profile, Delegate* delegate) {
  return new ExtensionUninstallDialogViews(profile, delegate);
}
