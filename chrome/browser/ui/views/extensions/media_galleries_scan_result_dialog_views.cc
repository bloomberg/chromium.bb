// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_scan_result_dialog_views.h"

#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

typedef MediaGalleriesScanResultDialogController::OrderedScanResults
    OrderedScanResults;

namespace {

// TODO(vandebo) move this to a common place.
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

MediaGalleriesScanResultDialogViews::MediaGalleriesScanResultDialogViews(
    MediaGalleriesScanResultDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(new views::View()),
      accepted_(false) {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. |window_| will take
  // care of deleting itself after calling DeleteDelegate().
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          controller->web_contents());
  DCHECK(web_contents_modal_dialog_manager);
  web_modal::WebContentsModalDialogManagerDelegate* modal_delegate =
      web_contents_modal_dialog_manager->delegate();
  DCHECK(modal_delegate);
  window_ = views::Widget::CreateWindowAsFramelessChild(
      this, modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowDialog(window_->GetNativeView());
}

MediaGalleriesScanResultDialogViews::~MediaGalleriesScanResultDialogViews() {}

void MediaGalleriesScanResultDialogViews::InitChildViews() {
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
  scroll_container->SetBorder(views::Border::CreateEmptyBorder(
      views::kRelatedControlVerticalSpacing,
      0,
      views::kRelatedControlVerticalSpacing,
      0));

  // Add attached galleries checkboxes.
  gallery_view_map_.clear();
  OrderedScanResults scan_results = controller_->GetGalleryList();
  for (OrderedScanResults::const_iterator it = scan_results.begin();
       it != scan_results.end();
       ++it) {
    int spacing = 0;
    if (it + 1 == scan_results.end())
      spacing = views::kRelatedControlSmallVerticalSpacing;
    AddOrUpdateScanResult(it->pref_info, it->selected, scroll_container,
                          spacing);
  }

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

void MediaGalleriesScanResultDialogViews::UpdateResults() {
  InitChildViews();
  contents_->Layout();
}

bool MediaGalleriesScanResultDialogViews::AddOrUpdateScanResult(
    const MediaGalleryPrefInfo& gallery,
    bool selected,
    views::View* container,
    int trailing_vertical_space) {
  base::string16 label = gallery.GetGalleryDisplayName();
  base::string16 tooltip_text = gallery.GetGalleryTooltip();
  base::string16 details = gallery.GetGalleryAdditionalDetails();
  bool is_attached = gallery.IsGalleryAvailable();

  GalleryViewMap::iterator it = gallery_view_map_.find(gallery.pref_id);
  if (it != gallery_view_map_.end()) {
    views::Checkbox* checkbox = it->second.checkbox;
    checkbox->SetChecked(selected);
    checkbox->SetText(label);
    checkbox->SetElideBehavior(views::Label::ELIDE_IN_MIDDLE);
    checkbox->SetTooltipText(tooltip_text);
    // Replace the details string.
    it->second.folder_viewer_button->SetVisible(is_attached);
    it->second.secondary_text->SetText(details);
    return false;
  }

  views::Checkbox* checkbox = new views::Checkbox(label);
  checkbox->set_listener(this);
  checkbox->set_context_menu_controller(this);
  checkbox->SetTooltipText(tooltip_text);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* folder_viewer_button = new views::ImageButton(this);
  folder_viewer_button->set_context_menu_controller(this);
  folder_viewer_button->SetImage(views::ImageButton::STATE_NORMAL,
                                 rb.GetImageSkiaNamed(IDR_FILE_FOLDER));
  folder_viewer_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                          views::ImageButton::ALIGN_MIDDLE);
  folder_viewer_button->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MEDIA_GALLERIES_SCAN_RESULT_OPEN_FOLDER_VIEW_ACCESSIBILITY_NAME));
  folder_viewer_button->SetFocusable(true);
  folder_viewer_button->SetVisible(is_attached);

  views::Label* secondary_text = new views::Label(details);
  secondary_text->set_context_menu_controller(this);
  secondary_text->SetTooltipText(tooltip_text);
  secondary_text->SetEnabledColor(kDeemphasizedTextColor);
  secondary_text->SetTooltipText(tooltip_text);
  secondary_text->SetBorder(views::Border::CreateEmptyBorder(
      0,
      views::kRelatedControlSmallHorizontalSpacing,
      0,
      views::kRelatedControlSmallHorizontalSpacing));

  views::View* checkbox_view = new views::View();
  checkbox_view->SetBorder(views::Border::CreateEmptyBorder(
      0, views::kPanelHorizMargin, trailing_vertical_space, 0));
  checkbox_view->set_context_menu_controller(this);
  checkbox_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  checkbox_view->AddChildView(checkbox);
  checkbox_view->AddChildView(folder_viewer_button);
  checkbox_view->AddChildView(secondary_text);

  container->AddChildView(checkbox_view);

  checkbox->SetChecked(selected);
  gallery_view_map_[gallery.pref_id].checkbox = checkbox;
  gallery_view_map_[gallery.pref_id].folder_viewer_button =
      folder_viewer_button;
  gallery_view_map_[gallery.pref_id].secondary_text = secondary_text;

  return true;
}

base::string16 MediaGalleriesScanResultDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
}

void MediaGalleriesScanResultDialogViews::DeleteDelegate() {
  controller_->DialogFinished(accepted_);
}

views::Widget* MediaGalleriesScanResultDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* MediaGalleriesScanResultDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* MediaGalleriesScanResultDialogViews::GetContentsView() {
  return contents_;
}

base::string16 MediaGalleriesScanResultDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK ?
      IDS_MEDIA_GALLERIES_DIALOG_CONFIRM :
      IDS_MEDIA_GALLERIES_DIALOG_CANCEL);
}

ui::ModalType MediaGalleriesScanResultDialogViews::GetModalType() const {
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

bool MediaGalleriesScanResultDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesScanResultDialogViews::Accept() {
  accepted_ = true;

  return true;
}

void MediaGalleriesScanResultDialogViews::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();

  for (GalleryViewMap::const_iterator it = gallery_view_map_.begin();
       it != gallery_view_map_.end(); ++it) {
    if (sender == it->second.checkbox) {
      controller_->DidToggleGalleryId(it->first,
                                      it->second.checkbox->checked());
      return;
    }
    if (sender == it->second.folder_viewer_button) {
      controller_->DidClickOpenFolderViewer(it->first);
      return;
    }
  }
}

void MediaGalleriesScanResultDialogViews::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  for (GalleryViewMap::const_iterator it = gallery_view_map_.begin();
       it != gallery_view_map_.end(); ++it) {
    if (it->second.checkbox->parent()->Contains(source)) {
      ShowContextMenu(point, source_type, it->first);
      return;
    }
  }
  NOTREACHED();
}

void MediaGalleriesScanResultDialogViews::ShowContextMenu(
    const gfx::Point& point,
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

void MediaGalleriesScanResultDialogViews::AcceptDialogForTesting() {
  accepted_ = true;

  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          controller_->web_contents());
  DCHECK(web_contents_modal_dialog_manager);
  web_modal::WebContentsModalDialogManager::TestApi(
      web_contents_modal_dialog_manager).CloseAllDialogs();
}

// MediaGalleriesScanResultDialogViewsController -------------------------------

// static
MediaGalleriesScanResultDialog* MediaGalleriesScanResultDialog::Create(
    MediaGalleriesScanResultDialogController* controller) {
  return new MediaGalleriesScanResultDialogViews(controller);
}
