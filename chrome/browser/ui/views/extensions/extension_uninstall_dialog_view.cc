// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
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

// TODO(estade): remove this when UseNewStyle() is the default.
int HorizontalMargin() {
  return views::DialogDelegate::UseNewStyle() ? views::kButtonHEdgeMarginNew :
                                                views::kPanelHorizMargin;
}

// Returns parent window for extension uninstall dialog.
gfx::NativeWindow GetParent(Browser* browser) {
  if (browser && browser->window())
    return browser->window()->GetNativeWindow();
  return NULL;
}

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews : public ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(Profile* profile,
                                Browser* browser,
                                ExtensionUninstallDialog::Delegate* delegate);
  virtual ~ExtensionUninstallDialogViews();

  // Forwards the accept and cancels to the delegate.
  void ExtensionUninstallAccepted();
  void ExtensionUninstallCanceled();

  ExtensionUninstallDialogDelegateView* view() { return view_; }

 private:
  virtual void Show() OVERRIDE;

  ExtensionUninstallDialogDelegateView* view_;
  bool show_in_app_list_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogViews);
};

// The dialog's view, owned by the views framework.
class ExtensionUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  ExtensionUninstallDialogDelegateView(
      ExtensionUninstallDialogViews* dialog_view,
      const extensions::Extension* extension,
      gfx::ImageSkia* icon);
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
    Profile* profile,
    Browser* browser,
    ExtensionUninstallDialog::Delegate* delegate)
    : ExtensionUninstallDialog(profile, browser, delegate),
      view_(NULL),
      show_in_app_list_(!browser) {
}

ExtensionUninstallDialogViews::~ExtensionUninstallDialogViews() {
  // Close the widget (the views framework will delete view_).
  if (view_) {
    view_->DialogDestroyed();
    view_->GetWidget()->CloseNow();
  }
}

void ExtensionUninstallDialogViews::Show() {
  gfx::NativeWindow parent = show_in_app_list_ ?
      AppListService::Get()->GetAppListWindow() :
      GetParent(browser_);
  if (browser_ && !parent) {
    delegate_->ExtensionUninstallCanceled();
    return;
  }

  view_ = new ExtensionUninstallDialogDelegateView(this, extension_, &icon_);
  CreateBrowserModalDialogViews(view_, parent)->Show();
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
    const extensions::Extension* extension,
    gfx::ImageSkia* icon)
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
  width += HorizontalMargin() * 2;
  width += views::kRelatedControlHorizontalSpacing;

  int height = views::kPanelVertMargin * 2;
  height += heading_->GetHeightForWidth(kRightColumnWidth);

  return gfx::Size(width,
                   std::max(height, kIconSize + views::kPanelVertMargin * 2));
}

void ExtensionUninstallDialogDelegateView::Layout() {
  int x = HorizontalMargin();
  int y = views::kPanelVertMargin;

  heading_->SizeToFit(kRightColumnWidth);

  if (heading_->height() <= kIconSize) {
    icon_->SetBounds(x, y, kIconSize, kIconSize);
    x += kIconSize;
    x += views::kRelatedControlHorizontalSpacing;

    heading_->SetX(x);
    heading_->SetY(y + (kIconSize - heading_->height()) / 2);
  } else {
    icon_->SetBounds(x,
                     y + (heading_->height() - kIconSize) / 2,
                     kIconSize,
                     kIconSize);
    x += kIconSize;
    x += views::kRelatedControlHorizontalSpacing;

    heading_->SetX(x);
    heading_->SetY(y);
  }
}

}  // namespace

// static
ExtensionUninstallDialog* ExtensionUninstallDialog::Create(
    Profile* profile,
    Browser* browser,
    Delegate* delegate) {
  return new ExtensionUninstallDialogViews(profile, browser, delegate);
}
