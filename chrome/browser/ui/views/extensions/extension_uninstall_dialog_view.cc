// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "extensions/common/extension.h"
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

namespace {

const int kRightColumnWidth = 210;
const int kIconSize = 64;

class ExtensionUninstallDialogDelegateView;

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews
    : public extensions::ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(
      Profile* profile,
      gfx::NativeWindow parent,
      extensions::ExtensionUninstallDialog::Delegate* delegate);
  virtual ~ExtensionUninstallDialogViews();

  // Called when the ExtensionUninstallDialogDelegate has been destroyed to make
  // sure we invalidate pointers.
  void DialogDelegateDestroyed() { view_ = NULL; }

  // Forwards the accept and cancels to the delegate.
  void ExtensionUninstallAccepted();
  void ExtensionUninstallCanceled();

 private:
  virtual void Show() OVERRIDE;

  ExtensionUninstallDialogDelegateView* view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogViews);
};

// The dialog's view, owned by the views framework.
class ExtensionUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  ExtensionUninstallDialogDelegateView(
      ExtensionUninstallDialogViews* dialog_view,
      const extensions::Extension* extension,
      const extensions::Extension* triggering_extension,
      gfx::ImageSkia* image);
  virtual ~ExtensionUninstallDialogDelegateView();

  // Called when the ExtensionUninstallDialog has been destroyed to make sure
  // we invalidate pointers.
  void DialogDestroyed() { dialog_ = NULL; }

 private:
  // views::DialogDelegate:
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE {
    // Default to accept when triggered via chrome://extensions page.
    return triggered_by_extension_ ?
        ui::DIALOG_BUTTON_CANCEL : ui::DIALOG_BUTTON_OK;
  }
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_WINDOW;
  }
  virtual base::string16 GetWindowTitle() const OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  virtual void Layout() OVERRIDE;

  ExtensionUninstallDialogViews* dialog_;

  views::ImageView* icon_;
  views::Label* heading_;
  bool triggered_by_extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogDelegateView);
};

ExtensionUninstallDialogViews::ExtensionUninstallDialogViews(
    Profile* profile,
    gfx::NativeWindow parent,
    extensions::ExtensionUninstallDialog::Delegate* delegate)
    : extensions::ExtensionUninstallDialog(profile, parent, delegate),
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
  view_ = new ExtensionUninstallDialogDelegateView(
      this, extension_, triggering_extension_, &icon_);
  CreateBrowserModalDialogViews(view_, parent_)->Show();
}

void ExtensionUninstallDialogViews::ExtensionUninstallAccepted() {
  // The widget gets destroyed when the dialog is accepted.
  view_->DialogDestroyed();
  view_ = NULL;
  delegate_->ExtensionUninstallAccepted();
}

void ExtensionUninstallDialogViews::ExtensionUninstallCanceled() {
  // The widget gets destroyed when the dialog is canceled.
  view_->DialogDestroyed();
  view_ = NULL;
  delegate_->ExtensionUninstallCanceled();
}

ExtensionUninstallDialogDelegateView::ExtensionUninstallDialogDelegateView(
    ExtensionUninstallDialogViews* dialog_view,
    const extensions::Extension* extension,
    const extensions::Extension* triggering_extension,
    gfx::ImageSkia* image)
    : dialog_(dialog_view),
      triggered_by_extension_(triggering_extension != NULL) {
  // Scale down to icon size, but allow smaller icons (don't scale up).
  gfx::Size size(image->width(), image->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  icon_ = new views::ImageView();
  icon_->SetImageSize(size);
  icon_->SetImage(*image);
  AddChildView(icon_);

  heading_ = new views::Label(base::UTF8ToUTF16(dialog_->GetHeadingText()));
  heading_->SetMultiLine(true);
  heading_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(heading_);
}

ExtensionUninstallDialogDelegateView::~ExtensionUninstallDialogDelegateView() {
  // If we're here, 2 things could have happened. Either the user closed the
  // dialog nicely and one of ExtensionUninstallAccepted or
  // ExtensionUninstallCanceled has been called (in which case dialog_ will be
  // NULL), *or* neither of them have been called and we are being forced closed
  // by our parent widget. In this case, we need to make sure to notify dialog_
  // not to call us again, since we're about to be freed by the Widget
  // framework.
  if (dialog_)
    dialog_->DialogDelegateDestroyed();
}

base::string16 ExtensionUninstallDialogDelegateView::GetDialogButtonLabel(
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

base::string16 ExtensionUninstallDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
}

gfx::Size ExtensionUninstallDialogDelegateView::GetPreferredSize() const {
  int width = kRightColumnWidth;
  width += kIconSize;
  width += views::kButtonHEdgeMarginNew * 2;
  width += views::kRelatedControlHorizontalSpacing;

  int height = views::kPanelVertMargin * 2;
  height += heading_->GetHeightForWidth(kRightColumnWidth);

  return gfx::Size(width,
                   std::max(height, kIconSize + views::kPanelVertMargin * 2));
}

void ExtensionUninstallDialogDelegateView::Layout() {
  int x = views::kButtonHEdgeMarginNew;
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
extensions::ExtensionUninstallDialog*
extensions::ExtensionUninstallDialog::Create(Profile* profile,
                                             gfx::NativeWindow parent,
                                             Delegate* delegate) {
  return new ExtensionUninstallDialogViews(profile, parent, delegate);
}
