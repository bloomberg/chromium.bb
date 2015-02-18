// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/bookmark_app_bubble_view.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::ColumnSet;
using views::GridLayout;

namespace {

// Minimum width of the the bubble.
const int kMinBubbleWidth = 300;
// Minimum width of the the textfield.
const int kMinTextfieldWidth = 200;
// Size of the icon.
const int kIconSize = extension_misc::EXTENSION_ICON_MEDIUM;

class WebAppInfoImageSource : public gfx::ImageSkiaSource {
 public:
  WebAppInfoImageSource(int dip_size, const WebApplicationInfo& info)
      : dip_size_(dip_size), info_(info) {}
  ~WebAppInfoImageSource() override {}

 private:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    int size = gfx::ClampToInt(dip_size_ * scale);
    for (const auto& icon_info : info_.icons) {
      if (icon_info.width == size) {
        return gfx::ImageSkiaRep(icon_info.data, scale);
      }
    }
    return gfx::ImageSkiaRep();
  }

  int dip_size_;
  WebApplicationInfo info_;
};

}  // namespace

BookmarkAppBubbleView::~BookmarkAppBubbleView() {
}

// static
void BookmarkAppBubbleView::ShowBubble(
    views::View* anchor_view,
    const WebApplicationInfo& web_app_info,
    const BrowserWindow::ShowBookmarkAppBubbleCallback& callback) {
  // |bookmark_app_bubble| becomes owned by the BubbleDelegateView through the
  // views system, and is freed when the BubbleDelegateView is closed and
  // subsequently destroyed.
  BookmarkAppBubbleView* bookmark_app_bubble =
      new BookmarkAppBubbleView(anchor_view, web_app_info, callback);
  views::BubbleDelegateView::CreateBubble(bookmark_app_bubble)->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_app_bubble->title_tf_->SelectAll(true);
  bookmark_app_bubble->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
}

BookmarkAppBubbleView::BookmarkAppBubbleView(
    views::View* anchor_view,
    const WebApplicationInfo& web_app_info,
    const BrowserWindow::ShowBookmarkAppBubbleCallback& callback)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_app_info_(web_app_info),
      user_accepted_(false),
      callback_(callback),
      add_button_(NULL),
      cancel_button_(NULL),
      open_as_window_checkbox_(NULL),
      title_tf_(NULL) {
  const SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  set_arrow(views::BubbleBorder::TOP_CENTER);
  set_color(background_color);
  set_background(views::Background::CreateSolidBackground(background_color));
  set_margins(gfx::Insets(views::kPanelVertMargin, 0, 0, 0));
}

void BookmarkAppBubbleView::Init() {
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(TitleStringId()));
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::MediumFont));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  add_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_ADD));
  add_button_->SetStyle(views::Button::STYLE_BUTTON);
  add_button_->SetIsDefault(true);

  cancel_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Column sets used in the layout of the bubble.
  enum ColumnSetID {
    TITLE_COLUMN_SET_ID,
    TITLE_TEXT_COLUMN_SET_ID,
    CONTENT_COLUMN_SET_ID
  };

  // The column layout used for the title and checkbox.
  ColumnSet* cs = layout->AddColumnSet(TITLE_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // The column layout used for the icon and text box.
  cs = layout->AddColumnSet(TITLE_TEXT_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::LEADING,
                GridLayout::CENTER,
                0,
                GridLayout::USE_PREF,
                0,
                0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::FILL,
                GridLayout::CENTER,
                1,
                GridLayout::USE_PREF,
                0,
                kMinTextfieldWidth);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // The column layout used for the row with buttons.
  cs = layout->AddColumnSet(CONTENT_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(
      GridLayout::LEADING, GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  layout->StartRow(0, TITLE_COLUMN_SET_ID);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, TITLE_TEXT_COLUMN_SET_ID);
  icon_image_view_ = new views::ImageView();

  gfx::Size image_size(kIconSize, kIconSize);
  gfx::ImageSkia image(new WebAppInfoImageSource(kIconSize, web_app_info_),
                       image_size);
  icon_image_view_->SetImageSize(image_size);
  icon_image_view_->SetImage(image);
  layout->AddView(icon_image_view_);

  title_tf_ = new views::Textfield();
  title_tf_->SetText(web_app_info_.title);
  title_tf_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_APP_AX_BUBBLE_NAME_LABEL));
  title_tf_->set_controller(this);
  layout->AddView(title_tf_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, CONTENT_COLUMN_SET_ID);
  open_as_window_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_WINDOW));
  open_as_window_checkbox_->SetChecked(web_app_info_.open_as_window);
  layout->AddView(open_as_window_checkbox_);
  layout->AddView(add_button_);
  layout->AddView(cancel_button_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  UpdateAddButtonState();
}

views::View* BookmarkAppBubbleView::GetInitiallyFocusedView() {
  return title_tf_;
}

void BookmarkAppBubbleView::WindowClosing() {
  callback_.Run(user_accepted_, web_app_info_);
}

bool BookmarkAppBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN) {
    HandleButtonPressed(add_button_);
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void BookmarkAppBubbleView::GetAccessibleState(ui::AXViewState* state) {
  views::BubbleDelegateView::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(TitleStringId());
}

gfx::Size BookmarkAppBubbleView::GetMinimumSize() const {
  gfx::Size size(views::BubbleDelegateView::GetPreferredSize());
  size.SetToMax(gfx::Size(kMinBubbleWidth, 0));
  return size;
}

void BookmarkAppBubbleView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  HandleButtonPressed(sender);
}

void BookmarkAppBubbleView::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(title_tf_, sender);
  UpdateAddButtonState();
}

void BookmarkAppBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == add_button_) {
    user_accepted_ = true;
    web_app_info_.title = GetTrimmedTitle();
    web_app_info_.open_as_window = open_as_window_checkbox_->checked();
  }

  GetWidget()->Close();
}

void BookmarkAppBubbleView::UpdateAddButtonState() {
  add_button_->SetEnabled(!GetTrimmedTitle().empty());
}

int BookmarkAppBubbleView::TitleStringId() {
#if defined(OS_WIN)
    int string_id = IDS_ADD_TO_TASKBAR_BUBBLE_TITLE;
#else
    int string_id = IDS_ADD_TO_DESKTOP_BUBBLE_TITLE;
#endif
#if defined(USE_ASH)
    if (chrome::GetHostDesktopTypeForNativeWindow(
            anchor_widget()->GetNativeWindow()) ==
        chrome::HOST_DESKTOP_TYPE_ASH) {
      string_id = IDS_ADD_TO_SHELF_BUBBLE_TITLE;
    }
#endif
    return string_id;
}

base::string16 BookmarkAppBubbleView::GetTrimmedTitle() {
  base::string16 title(title_tf_->text());
  base::TrimWhitespace(title, base::TRIM_ALL, &title);
  return title;
}
