// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

using web_modal::WebContentsModalDialogManager;
using web_modal::WebContentsModalDialogManagerDelegate;

namespace {

const int kScrollAreaHeight = 192;

// This container has the right Layout() impl to use within a ScrollView.
class ScrollableView : public views::View {
 public:
  ScrollableView() {}
  virtual ~ScrollableView() {}

  virtual void Layout() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollableView);
};

void ScrollableView::Layout() {
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = parent()->width();
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);

  views::View::Layout();
}

}  // namespace

typedef MediaGalleriesDialogController::GalleryPermissionsVector
    GalleryPermissionsVector;

MediaGalleriesDialogViews::MediaGalleriesDialogViews(
    MediaGalleriesDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(new views::View()),
      add_gallery_button_(NULL),
      confirm_available_(false),
      accepted_(false) {
  InitChildViews();

  if (ControllerHasWebContents()) {
    // Ownership of |contents_| is handed off by this call. |window_| will take
    // care of deleting itself after calling DeleteDelegate().
    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(
            controller->web_contents());
    DCHECK(web_contents_modal_dialog_manager);
    WebContentsModalDialogManagerDelegate* modal_delegate =
        web_contents_modal_dialog_manager->delegate();
    DCHECK(modal_delegate);
    window_ = views::Widget::CreateWindowAsFramelessChild(
        this, modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
    web_contents_modal_dialog_manager->ShowModalDialog(
        window_->GetNativeView());
  }
}

MediaGalleriesDialogViews::~MediaGalleriesDialogViews() {
  if (!ControllerHasWebContents())
    delete contents_;
}

void MediaGalleriesDialogViews::InitChildViews() {
  // Outer dialog layout.
  contents_->RemoveAllChildViews(true);
  int dialog_content_width = views::Widget::GetLocalizedContentsWidth(
      IDS_MEDIA_GALLERIES_DIALOG_CONTENT_WIDTH_CHARS);
  views::GridLayout* layout = views::GridLayout::CreatePanel(contents_);
  contents_->SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     1,
                     views::GridLayout::FIXED,
                     dialog_content_width,
                     0);

  // Message text.
  views::Label* subtext = new views::Label(controller_->GetSubtext());
  subtext->SetMultiLine(true);
  subtext->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, column_set_id);
  layout->AddView(
      subtext, 1, 1,
      views::GridLayout::FILL, views::GridLayout::LEADING,
      dialog_content_width, subtext->GetHeightForWidth(dialog_content_width));
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Scrollable area for checkboxes.
  ScrollableView* scroll_container = new ScrollableView();
  scroll_container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0,
      views::kRelatedControlSmallVerticalSpacing));
  scroll_container->SetBorder(
      views::Border::CreateEmptyBorder(views::kRelatedControlVerticalSpacing,
                                       0,
                                       views::kRelatedControlVerticalSpacing,
                                       0));

  // Add attached galleries checkboxes.
  checkbox_map_.clear();
  new_checkbox_map_.clear();
  GalleryPermissionsVector permissions = controller_->AttachedPermissions();
  for (GalleryPermissionsVector::const_iterator iter = permissions.begin();
       iter != permissions.end(); ++iter) {
    int spacing = 0;
    if (iter + 1 == permissions.end())
      spacing = views::kRelatedControlSmallVerticalSpacing;
    AddOrUpdateGallery(iter->pref_info, iter->allowed, scroll_container,
                       spacing);
  }

  GalleryPermissionsVector unattached_permissions =
      controller_->UnattachedPermissions();

  if (!unattached_permissions.empty()) {
    // Separator line.
    views::Separator* separator = new views::Separator(
        views::Separator::HORIZONTAL);
    scroll_container->AddChildView(separator);

    // Unattached locations section.
    views::Label* unattached_text = new views::Label(
        controller_->GetUnattachedLocationsHeader());
    unattached_text->SetMultiLine(true);
    unattached_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    unattached_text->SetBorder(
        views::Border::CreateEmptyBorder(views::kRelatedControlVerticalSpacing,
                                         views::kPanelHorizMargin,
                                         views::kRelatedControlVerticalSpacing,
                                         0));
    scroll_container->AddChildView(unattached_text);

    // Add unattached galleries checkboxes.
    for (GalleryPermissionsVector::const_iterator iter =
             unattached_permissions.begin();
         iter != unattached_permissions.end(); ++iter) {
      AddOrUpdateGallery(iter->pref_info, iter->allowed, scroll_container, 0);
    }
  }

  confirm_available_ = controller_->HasPermittedGalleries();

  // Add the scrollable area to the outer dialog view. It will squeeze against
  // the title/subtitle and buttons to occupy all available space in the dialog.
  views::ScrollView* scroll_view =
      views::ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(scroll_container);
  layout->StartRowWithPadding(1, column_set_id,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(scroll_view, 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  dialog_content_width, kScrollAreaHeight);
}

void MediaGalleriesDialogViews::UpdateGalleries() {
  InitChildViews();
  contents_->Layout();
}

bool MediaGalleriesDialogViews::AddOrUpdateGallery(
    const MediaGalleryPrefInfo& gallery,
    bool permitted,
    views::View* container,
    int trailing_vertical_space) {
  base::string16 label = gallery.GetGalleryDisplayName();
  base::string16 tooltip_text = gallery.GetGalleryTooltip();
  base::string16 details = gallery.GetGalleryAdditionalDetails();

  CheckboxMap::iterator iter = checkbox_map_.find(gallery.pref_id);
  if (iter != checkbox_map_.end() &&
      gallery.pref_id != kInvalidMediaGalleryPrefId) {
    views::Checkbox* checkbox = iter->second->checkbox();
    checkbox->SetChecked(permitted);
    checkbox->SetText(label);
    checkbox->SetTooltipText(tooltip_text);
    iter->second->secondary_text()->SetText(details);
    iter->second->secondary_text()->SetVisible(details.length() > 0);
    return false;
  }

  views::ContextMenuController* menu_controller = NULL;
  if (gallery.pref_id != kInvalidMediaGalleryPrefId)
    menu_controller = this;

  MediaGalleryCheckboxView* gallery_view =
      new MediaGalleryCheckboxView(label, tooltip_text, details, false,
                                   trailing_vertical_space, this,
                                   menu_controller);
  gallery_view->checkbox()->SetChecked(permitted);
  container->AddChildView(gallery_view);

  if (gallery.pref_id != kInvalidMediaGalleryPrefId)
    checkbox_map_[gallery.pref_id] = gallery_view;
  else
    new_checkbox_map_[gallery_view] = gallery;

  return true;
}

base::string16 MediaGalleriesDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
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

base::string16 MediaGalleriesDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK ?
      IDS_MEDIA_GALLERIES_DIALOG_CONFIRM :
      IDS_MEDIA_GALLERIES_DIALOG_CANCEL);
}

bool MediaGalleriesDialogViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || confirm_available_;
}

ui::ModalType MediaGalleriesDialogViews::GetModalType() const {
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

views::View* MediaGalleriesDialogViews::CreateExtraView() {
  DCHECK(!add_gallery_button_);
  add_gallery_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY));
  add_gallery_button_->SetStyle(views::Button::STYLE_BUTTON);
  return add_gallery_button_;
}

bool MediaGalleriesDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesDialogViews::Accept() {
  accepted_ = true;

  return true;
}

void MediaGalleriesDialogViews::ButtonPressed(views::Button* sender,
                                              const ui::Event& /* event */) {
  confirm_available_ = true;

  if (ControllerHasWebContents())
    GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();

  if (sender == add_gallery_button_) {
    controller_->OnAddFolderClicked();
    return;
  }

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (sender == iter->second->checkbox()) {
      controller_->DidToggleGalleryId(iter->first,
                                      iter->second->checkbox()->checked());
      return;
    }
  }
  for (NewCheckboxMap::const_iterator iter = new_checkbox_map_.begin();
       iter != new_checkbox_map_.end(); ++iter) {
    if (sender == iter->first->checkbox()) {
      controller_->DidToggleNewGallery(iter->second,
                                       iter->first->checkbox()->checked());
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->second->Contains(source)) {
      ShowContextMenu(point, source_type, iter->first);
      return;
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenu(const gfx::Point& point,
                                                ui::MenuSourceType source_type,
                                                MediaGalleryPrefId id) {
  context_menu_runner_.reset(new views::MenuRunner(
      controller_->GetContextMenu(id)));

  if (context_menu_runner_->RunMenuAt(
          GetWidget(), NULL, gfx::Rect(point.x(), point.y(), 0, 0),
          views::MenuItemView::TOPLEFT, source_type,
          views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

bool MediaGalleriesDialogViews::ControllerHasWebContents() const {
  return controller_->web_contents() != NULL;
}

// MediaGalleriesDialogViewsController -----------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogViews(controller);
}
