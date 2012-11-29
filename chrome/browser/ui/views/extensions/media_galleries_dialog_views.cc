// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"


namespace chrome {

typedef MediaGalleriesDialogController::KnownGalleryPermissions
    GalleryPermissions;

namespace {

// Heading font size correction.
const int kHeadingFontSizeDelta = 1;

}  // namespace

MediaGalleriesDialogViews::MediaGalleriesDialogViews(
    MediaGalleriesDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(new views::View()),
      checkbox_container_(NULL),
      add_gallery_container_(NULL),
      confirm_available_(false),
      accepted_(false),
      // Enable this once chrome style constrained windows work on shell
      // windows. http://crbug.com/156694
      enable_chrome_style_(UseChromeStyleDialogs()) {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. |window_| will take
  // care of deleting itself after calling DeleteDelegate().
  window_ = new ConstrainedWindowViews(
      controller->web_contents(), this,
      enable_chrome_style_,
      ConstrainedWindowViews::DEFAULT_INSETS);
}

MediaGalleriesDialogViews::~MediaGalleriesDialogViews() {}

void MediaGalleriesDialogViews::InitChildViews() {
  // Layout.
  views::GridLayout* layout = new views::GridLayout(contents_);
  if (!enable_chrome_style_) {
    layout->SetInsets(views::kPanelVertMargin, views::kPanelHorizMargin,
                      0, views::kPanelHorizMargin);
  }
  int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     1,
                     views::GridLayout::FIXED,
                     views::Widget::GetLocalizedContentsWidth(
                         IDS_MEDIA_GALLERIES_DIALOG_CONTENT_WIDTH_CHARS),
                     0);
  contents_->SetLayoutManager(layout);

  // Header text.
  if (!enable_chrome_style_) {
    views::Label* header = new views::Label(controller_->GetHeader());
    header->SetFont(header->font().DeriveFont(kHeadingFontSizeDelta,
                                              gfx::Font::BOLD));
    header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0, column_set_id);
    layout->AddView(header);
  }

  // Message text.
  views::Label* subtext = new views::Label(controller_->GetSubtext());
  subtext->SetMultiLine(true);
  subtext->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, column_set_id,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(subtext);

  // Checkboxes.
  checkbox_container_ = new views::View();
  checkbox_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           0, 0,
                           views::kRelatedControlSmallVerticalSpacing));
  layout->StartRowWithPadding(0, column_set_id,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(checkbox_container_);

  const GalleryPermissions& permissions = controller_->permissions();
  for (GalleryPermissions::const_iterator iter = permissions.begin();
       iter != permissions.end(); ++iter) {
    AddOrUpdateGallery(&iter->second.pref_info, iter->second.allowed);
  }
  confirm_available_ = controller_->HasPermittedGalleries();

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
}

void MediaGalleriesDialogViews::UpdateGallery(
    const MediaGalleryPrefInfo* gallery,
    bool permitted) {
  // After adding a new checkbox, we have to update the size of the dialog.
  if (AddOrUpdateGallery(gallery, permitted))
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

bool MediaGalleriesDialogViews::AddOrUpdateGallery(
    const MediaGalleryPrefInfo* gallery,
    bool permitted) {
  string16 label =
      MediaGalleriesDialogController::GetGalleryDisplayName(*gallery);
  CheckboxMap::iterator iter = checkbox_map_.find(gallery);
  if (iter != checkbox_map_.end()) {
    views::Checkbox* checkbox = iter->second;
    checkbox->SetChecked(permitted);
    checkbox->SetText(label);
    return false;
  }

  views::Checkbox* checkbox = new views::Checkbox(label);
  checkbox->set_listener(this);
  checkbox->SetTooltipText(
      MediaGalleriesDialogController::GetGalleryTooltip(*gallery));
  checkbox_container_->AddChildView(checkbox);
  checkbox->SetChecked(permitted);
  checkbox_map_[gallery] = checkbox;

  return true;
}

string16 MediaGalleriesDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
}

bool MediaGalleriesDialogViews::ShouldShowWindowTitle() const {
  return enable_chrome_style_;
}

void MediaGalleriesDialogViews::DeleteDelegate() {
  controller_->DialogFinished(accepted_);
}

views::Widget* MediaGalleriesDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* MediaGalleriesDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* MediaGalleriesDialogViews::GetContentsView() {
  return contents_;
}

string16 MediaGalleriesDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK ?
      IDS_MEDIA_GALLERIES_DIALOG_CONFIRM :
      IDS_MEDIA_GALLERIES_DIALOG_CANCEL);
}

bool MediaGalleriesDialogViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || confirm_available_;
}

bool MediaGalleriesDialogViews::UseChromeStyle() const {
  return enable_chrome_style_;
}

ui::ModalType MediaGalleriesDialogViews::GetModalType() const {
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

views::View* MediaGalleriesDialogViews::GetExtraView() {
  if (!add_gallery_container_) {
    views::View* button = new views::NativeTextButton(this,
        l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY));
    add_gallery_container_ = new views::View();
    add_gallery_container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
    add_gallery_container_->AddChildView(button);
  }

  return add_gallery_container_;
}

bool MediaGalleriesDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesDialogViews::Accept() {
  accepted_ = true;
  return true;
}

void MediaGalleriesDialogViews::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  confirm_available_ = true;
  GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();

  if (sender->parent() == add_gallery_container_) {
    controller_->OnAddFolderClicked();
    return;
  }

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (sender == iter->second) {
      controller_->DidToggleGallery(
          iter->first, static_cast<views::Checkbox*>(sender)->checked());
      return;
    }
  }

  NOTREACHED();
}

// MediaGalleriesDialogViewsController -----------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogViews(controller);
}

}  // namespace chrome
