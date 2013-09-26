// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
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

// Equal to the #969696 color used in spec (note WebUI color is #999).
const SkColor kDeemphasizedTextColor = SkColorSetRGB(159, 159, 159);

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
    width = std::max(parent()->width(), width);
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
      this,
      controller->web_contents()->GetView()->GetNativeView(),
      modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowDialog(window_->GetNativeView());
  web_contents_modal_dialog_manager->SetCloseOnInterstitialWebUI(
      window_->GetNativeView(), true);
}

MediaGalleriesDialogViews::~MediaGalleriesDialogViews() {}

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

  if (!DialogDelegate::UseNewStyle()) {
    // Header text.
    views::Label* header = new views::Label(controller_->GetHeader());
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    header->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
    header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0, column_set_id);
    layout->AddView(header);
  }

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
  scroll_container->set_border(views::Border::CreateEmptyBorder(
      views::kRelatedControlVerticalSpacing,
      0,
      views::kRelatedControlVerticalSpacing,
      0));

  // Add attached galleries checkboxes.
  checkbox_map_.clear();
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
    unattached_text->set_border(views::Border::CreateEmptyBorder(
        views::kRelatedControlVerticalSpacing,
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

void MediaGalleriesDialogViews::UpdateGallery(
    const MediaGalleryPrefInfo& gallery,
    bool permitted) {
  InitChildViews();
  contents_->Layout();
}

void MediaGalleriesDialogViews::ForgetGallery(MediaGalleryPrefId gallery) {
  InitChildViews();
  contents_->Layout();
}

bool MediaGalleriesDialogViews::AddOrUpdateGallery(
    const MediaGalleryPrefInfo& gallery,
    bool permitted,
    views::View* container,
    int trailing_vertical_space) {
  string16 label = gallery.GetGalleryDisplayName();
  string16 tooltip_text = gallery.GetGalleryTooltip();
  string16 details = gallery.GetGalleryAdditionalDetails();

  CheckboxMap::iterator iter = checkbox_map_.find(gallery.pref_id);
  if (iter != checkbox_map_.end()) {
    views::Checkbox* checkbox = iter->second;
    checkbox->SetChecked(permitted);
    checkbox->SetText(label);
    checkbox->SetElideBehavior(views::Label::ELIDE_IN_MIDDLE);
    checkbox->SetTooltipText(tooltip_text);
    // Replace the details string.
    views::View* checkbox_view = checkbox->parent();
    DCHECK_EQ(2, checkbox_view->child_count());
    views::Label* secondary_text =
        static_cast<views::Label*>(checkbox_view->child_at(1));
    secondary_text->SetText(details);

    // Why is this returning false? Looks like that will mean it doesn't paint.
    return false;
  }

  views::Checkbox* checkbox = new views::Checkbox(label);
  checkbox->set_listener(this);
  checkbox->SetTooltipText(tooltip_text);
  views::Label* secondary_text = new views::Label(details);
  secondary_text->SetTooltipText(tooltip_text);
  secondary_text->SetEnabledColor(kDeemphasizedTextColor);
  secondary_text->SetTooltipText(tooltip_text);
  secondary_text->set_border(views::Border::CreateEmptyBorder(
      0,
      views::kRelatedControlSmallHorizontalSpacing,
      0,
      views::kRelatedControlSmallHorizontalSpacing));

  views::View* checkbox_view = new views::View();
  checkbox_view->set_border(views::Border::CreateEmptyBorder(
      0,
      views::kPanelHorizMargin,
      trailing_vertical_space,
      0));
  checkbox_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  checkbox_view->AddChildView(checkbox);
  checkbox_view->AddChildView(secondary_text);

  container->AddChildView(checkbox_view);

  checkbox->SetChecked(permitted);
  checkbox_map_[gallery.pref_id] = checkbox;

  return true;
}

string16 MediaGalleriesDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
}

bool MediaGalleriesDialogViews::ShouldShowWindowTitle() const {
  return DialogDelegate::UseNewStyle();
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
  add_gallery_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  return add_gallery_button_;
}

bool MediaGalleriesDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesDialogViews::Accept() {
  accepted_ = true;

  return true;
}

// TODO(wittman): Remove this override once we move to the new style frame view
// on all dialogs.
views::NonClientFrameView* MediaGalleriesDialogViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return CreateConstrainedStyleNonClientFrameView(
      widget,
      controller_->web_contents()->GetBrowserContext());
}

void MediaGalleriesDialogViews::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  confirm_available_ = true;
  GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();

  if (sender == add_gallery_button_) {
    controller_->OnAddFolderClicked();
    return;
  }

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (sender == iter->second) {
      controller_->DidToggleGalleryId(iter->first,
                                      iter->second->checked());
      return;
    }
  }
}

// MediaGalleriesDialogViewsController -----------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogViews(controller);
}
