// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_details_view.h"

#include "ash/common/ash_view_ids.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "base/containers/adapters.h"
#include "base/timer/timer.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
namespace {

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

// The index of the horizontal rule below the title row.
const int kTitleRowSeparatorIndex = 1;

// A view that is used as ScrollView contents. It supports designating some of
// the children as sticky header rows. The sticky header rows are not scrolled
// above the top of the visible viewport until the next one "pushes" it up and
// are painted above other children. To indicate that a child is a sticky header
// row use set_id(VIEW_ID_STICKY_HEADER).
class ScrollContentsView : public views::View,
                           public views::ViewTargeterDelegate {
 public:
  ScrollContentsView()
      : box_layout_(new views::BoxLayout(
            views::BoxLayout::kVertical,
            0,
            0,
            UseMd() ? 0 : kContentsBetweenChildSpacingNonMd)) {
    SetEventTargeter(base::MakeUnique<views::ViewTargeter>(this));
    SetLayoutManager(box_layout_);
  }
  ~ScrollContentsView() override {}

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    PositionHeaderRows();
  }

  void PaintChildren(const ui::PaintContext& context) override {
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i)->id() != VIEW_ID_STICKY_HEADER && !child_at(i)->layer())
        child_at(i)->Paint(context);
    }
    bool did_draw_shadow = false;
    // Paint header rows above other children in Z-order.
    for (auto& header : headers_) {
      if (!header.view->layer())
        header.view->Paint(context);
      did_draw_shadow = PaintDelineation(header, context) || did_draw_shadow;
    }

    // Draw a shadow at the top of the viewport when scrolled, but only if a
    // header didn't already draw one. Overlap the shadow with the separator
    // that's below the header view so we don't get both a separator and a full
    // shadow.
    if (y() != 0 && !did_draw_shadow)
      DrawShadow(context, gfx::Rect(0, 0, width(), -y() - kSeparatorWidth));
  }

  void Layout() override {
    views::View::Layout();
    headers_.clear();
    for (int i = 0; i < child_count(); ++i) {
      views::View* view = child_at(i);
      if (view->id() == VIEW_ID_STICKY_HEADER)
        headers_.emplace_back(view);
    }
    PositionHeaderRows();
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (!details.is_add && details.parent == this) {
      headers_.erase(std::remove_if(headers_.begin(), headers_.end(),
                                    [details](const Header& header) {
                                      return header.view == details.child;
                                    }),
                     headers_.end());
    } else if (details.is_add && details.parent == this &&
               details.child == child_at(0)) {
      // We always want padding on the bottom of the scroll contents.
      // We only want padding on the top of the scroll contents if the first
      // child is not a header (in that case, the padding is built into the
      // header).
      DCHECK_EQ(box_layout_, GetLayoutManager());
      box_layout_->set_inside_border_insets(
          gfx::Insets(details.child->id() == VIEW_ID_STICKY_HEADER
                          ? 0
                          : kMenuSeparatorVerticalPadding,
                      0, kMenuSeparatorVerticalPadding, 0));
    }
  }

  // views::ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    // Give header rows first dibs on events.
    for (auto& header : headers_) {
      views::View* view = header.view;
      gfx::Rect local_to_header = rect;
      local_to_header.Offset(-view->x(), -view->y());
      if (ViewTargeterDelegate::DoesIntersectRect(view, local_to_header))
        return ViewTargeterDelegate::TargetForRect(view, local_to_header);
    }
    return ViewTargeterDelegate::TargetForRect(root, rect);
  }

 private:
  const SkColor kSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x1F);
  const int kShadowOffsetY = 2;
  const int kShadowBlur = 2;
  // TODO(fukino): Remove this constant once we stop maintaining pre-MD design.
  // crbug.com/614453.
  const int kContentsBetweenChildSpacingNonMd = 1;

  // A structure that keeps the original offset of each header between the
  // calls to Layout() to allow keeping track of which view should be sticky.
  struct Header {
    explicit Header(views::View* view)
        : view(view), natural_offset(view->y()) {}

    // A header View that can be decorated as sticky.
    views::View* view;

    // Offset from the top of ScrollContentsView to |view|'s original vertical
    // position.
    int natural_offset;
  };

  // Adjusts y-position of header rows allowing one or two rows to stick to the
  // top of the visible viewport.
  void PositionHeaderRows() {
    const int scroll_offset = -y();
    Header* previous_header = nullptr;
    for (auto& header : base::Reversed(headers_)) {
      views::View* header_view = header.view;
      if (header.natural_offset >= scroll_offset) {
        previous_header = &header;
        header_view->SetY(header.natural_offset);
        continue;
      }
      if (previous_header &&
          previous_header->view->y() < scroll_offset + header_view->height()) {
        // Lower header displacing the header above.
        header_view->SetY(previous_header->view->y() - header_view->height());
      } else {
        // A header becomes sticky.
        header_view->SetY(scroll_offset);
        header_view->Layout();
        header_view->SchedulePaint();
      }
      break;
    }
  }

  // Paints a separator for a header view. The separator can be a horizontal
  // rule or a horizontal shadow, depending on whether the header is sticking to
  // the top of the scroll viewport. The return value indicates whether a shadow
  // was drawn.
  bool PaintDelineation(const Header& header, const ui::PaintContext& context) {
    const View* view = header.view;
    const bool at_top = view->y() == -y();

    // If the header is where it normally belongs, draw a separator above.
    if (view->y() == header.natural_offset) {
      // But if the header is at the very top of the viewport, draw nothing.
      if (at_top)
        return false;

      // TODO(estade): look better at 1.5x scale.
      ui::PaintRecorder recorder(context, size());
      gfx::Canvas* canvas = recorder.canvas();
      gfx::Rect separator = view->bounds();
      separator.set_height(kSeparatorWidth);
      canvas->FillRect(separator, kSeparatorColor);
      return false;
    }

    // If the header is displaced but is not at the top of the viewport, it's
    // being pushed out by another header. Draw nothing.
    if (!at_top)
      return false;

    // Otherwise, draw a shadow below.
    DrawShadow(context,
               gfx::Rect(0, 0, view->width(), view->bounds().bottom()));
    return true;
  }

  // Draws a drop shadow below |shadowed_area|.
  void DrawShadow(const ui::PaintContext& context,
                  const gfx::Rect& shadowed_area) {
    ui::PaintRecorder recorder(context, size());
    gfx::Canvas* canvas = recorder.canvas();
    SkPaint paint;
    gfx::ShadowValues shadow;
    shadow.emplace_back(gfx::Vector2d(0, kShadowOffsetY), kShadowBlur,
                        kSeparatorColor);
    paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadow));
    paint.setAntiAlias(true);
    canvas->ClipRect(shadowed_area, SkRegion::kDifference_Op);
    canvas->DrawRect(shadowed_area, paint);
  }

  views::BoxLayout* box_layout_;

  // Header child views that stick to the top of visible viewport when scrolled.
  std::vector<Header> headers_;

  DISALLOW_COPY_AND_ASSIGN(ScrollContentsView);
};

// Constants for the title row in material design.
const int kTitleRowVerticalPadding = 4;
const int kTitleRowProgressBarHeight = 2;
const int kTitleRowPaddingTop = kTitleRowVerticalPadding;
const int kTitleRowPaddingBottom =
    kTitleRowVerticalPadding - kTitleRowProgressBarHeight;

class ScrollSeparator : public views::View {
 public:
  ScrollSeparator() {}

  ~ScrollSeparator() override {}

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(0, height() / 2, width(), 1), kBorderLightColor);
  }
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(1, kTrayPopupScrollSeparatorHeight);
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollSeparator);
};

}  // namespace

class ScrollBorder : public views::Border {
 public:
  ScrollBorder() {}
  ~ScrollBorder() override {}

  void set_visible(bool visible) { visible_ = visible; }

 private:
  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    if (!visible_)
      return;
    canvas->FillRect(gfx::Rect(0, view.height() - 1, view.width(), 1),
                     kBorderLightColor);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(0, 0, 1, 0); }

  gfx::Size GetMinimumSize() const override { return gfx::Size(0, 1); }

  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollBorder);
};

TrayDetailsView::TrayDetailsView(SystemTrayItem* owner)
    : owner_(owner),
      box_layout_(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0)),
      title_row_(nullptr),
      scroller_(nullptr),
      scroll_content_(nullptr),
      progress_bar_(nullptr),
      scroll_border_(nullptr),
      tri_view_(nullptr),
      label_(nullptr),
      back_button_(nullptr) {
  SetLayoutManager(box_layout_);
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
}

TrayDetailsView::~TrayDetailsView() {}

void TrayDetailsView::OnViewClicked(views::View* sender) {
  if (!UseMd() && title_row_ && sender == title_row_->content()) {
    TransitionToDefaultView();
    return;
  }

  HandleViewClicked(sender);
}

void TrayDetailsView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (UseMd() && sender == back_button_) {
    TransitionToDefaultView();
    return;
  }

  HandleButtonPressed(sender, event);
}

void TrayDetailsView::CreateTitleRow(int string_id) {
  DCHECK(!tri_view_);
  DCHECK(!title_row_);

  if (UseMd()) {
    tri_view_ = TrayPopupUtils::CreateDefaultRowView();

    back_button_ = CreateBackButton();
    tri_view_->AddView(TriView::Container::START, back_button_);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    label_ = TrayPopupUtils::CreateDefaultLabel();
    label_->SetText(rb.GetLocalizedString(string_id));
    UpdateStyle();
    tri_view_->AddView(TriView::Container::CENTER, label_);

    tri_view_->SetContainerVisible(TriView::Container::END, false);

    tri_view_->SetBorder(views::CreateEmptyBorder(kTitleRowPaddingTop, 0,
                                                  kTitleRowPaddingBottom, 0));
    AddChildViewAt(tri_view_, 0);
    views::Separator* separator =
        new views::Separator(views::Separator::HORIZONTAL);
    separator->SetColor(kHorizontalSeparatorColor);
    separator->SetPreferredSize(kSeparatorWidth);
    separator->SetBorder(views::CreateEmptyBorder(
        kTitleRowProgressBarHeight - kSeparatorWidth, 0, 0, 0));
    AddChildViewAt(separator, kTitleRowSeparatorIndex);
  } else {
    title_row_ = new SpecialPopupRow();
    title_row_->SetTextLabel(string_id, this);
    AddChildViewAt(title_row_, child_count());
  }

  CreateExtraTitleRowButtons();
  Layout();
}

void TrayDetailsView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new ScrollContentsView();
  scroller_ = new FixedSizedScrollView;
  scroller_->SetContentsView(scroll_content_);
  // Make the |scroller_| have a layer to clip |scroll_content_|'s children.
  // TODO(varkha): Make the sticky rows work with EnableViewPortLayer().
  scroller_->SetPaintToLayer(true);
  scroller_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  scroller_->layer()->SetMasksToBounds(true);

  // Note: |scroller_| takes ownership of |scroll_border_|.
  if (!UseMd()) {
    // In MD, the scroller is always the last thing, so this border is
    // unnecessary and reserves extra space we don't want.
    scroll_border_ = new ScrollBorder;
    scroller_->SetBorder(std::unique_ptr<views::Border>(scroll_border_));
  }

  AddChildView(scroller_);
  box_layout_->SetFlexForView(scroller_, 1);
}

void TrayDetailsView::AddScrollSeparator() {
  DCHECK(scroll_content_);
  // Do not draw the separator if it is the very first item
  // in the scrollable list.
  if (scroll_content_->has_children())
    scroll_content_->AddChildView(new ScrollSeparator);
}

void TrayDetailsView::Reset() {
  RemoveAllChildViews(true);
  title_row_ = nullptr;
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  progress_bar_ = nullptr;
  back_button_ = nullptr;
  label_ = nullptr;
  tri_view_ = nullptr;
}

void TrayDetailsView::ShowProgress(double value, bool visible) {
  DCHECK(UseMd());
  DCHECK(tri_view_);
  if (!progress_bar_) {
    progress_bar_ = new views::ProgressBar(kTitleRowProgressBarHeight);
    progress_bar_->SetVisible(false);
    AddChildViewAt(progress_bar_, kTitleRowSeparatorIndex + 1);
  }

  progress_bar_->SetValue(value);
  progress_bar_->SetVisible(visible);
  child_at(kTitleRowSeparatorIndex)->SetVisible(!visible);
}

views::CustomButton* TrayDetailsView::CreateSettingsButton(LoginStatus status) {
  DCHECK(UseMd());
  SystemMenuButton* button = new SystemMenuButton(
      this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
      IDS_ASH_STATUS_TRAY_SETTINGS);
  if (!TrayPopupUtils::CanOpenWebUISettings(status))
    button->SetState(views::Button::STATE_DISABLED);
  return button;
}

views::CustomButton* TrayDetailsView::CreateHelpButton(LoginStatus status) {
  DCHECK(UseMd());
  SystemMenuButton* button =
      new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                           kSystemMenuHelpIcon, IDS_ASH_STATUS_TRAY_HELP);
  if (!TrayPopupUtils::CanOpenWebUISettings(status))
    button->SetState(views::Button::STATE_DISABLED);
  return button;
}

void TrayDetailsView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (UseMd())
    UpdateStyle();
}

void TrayDetailsView::UpdateStyle() {
  if (!GetNativeTheme() || !label_)
    return;

  TrayPopupItemStyle style(GetNativeTheme(),
                           TrayPopupItemStyle::FontStyle::TITLE);
  style.SetupLabel(label_);
}

void TrayDetailsView::HandleViewClicked(views::View* view) {
  NOTREACHED();
}

void TrayDetailsView::HandleButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  NOTREACHED();
}

void TrayDetailsView::CreateExtraTitleRowButtons() {}

void TrayDetailsView::TransitionToDefaultView() {
  if (UseMd()) {
    if (back_button_ && back_button_->HasFocus())
      owner_->set_restore_focus(true);
  } else {
    if (title_row_ && title_row_->content() &&
        title_row_->content()->HasFocus()) {
      owner_->set_restore_focus(true);
    }
    DoTransitionToDefaultView();
    return;
  }

  const int transition_delay =
      GetTrayConstant(TRAY_POPUP_TRANSITION_TO_DEFAULT_DELAY);
  if (transition_delay <= 0) {
    DoTransitionToDefaultView();
    return;
  }

  transition_delay_timer_.reset(new base::OneShotTimer());
  transition_delay_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(transition_delay), this,
      &TrayDetailsView::DoTransitionToDefaultView);
}

void TrayDetailsView::DoTransitionToDefaultView() {
  transition_delay_timer_.reset();

  // Cache pointer to owner in this function scope. TrayDetailsView will be
  // deleted after called ShowDefaultView.
  SystemTrayItem* owner = owner_;
  owner->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  owner->set_restore_focus(false);
}

views::Button* TrayDetailsView::CreateBackButton() {
  DCHECK(UseMd());
  SystemMenuButton* button = new SystemMenuButton(
      this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuArrowBackIcon,
      IDS_ASH_STATUS_TRAY_PREVIOUS_MENU);
  return button;
}

void TrayDetailsView::Layout() {
  if (UseMd()) {
    views::View::Layout();
    if (scroller_ && !scroller_->is_bounded())
      scroller_->ClipHeightTo(0, scroller_->height());
    return;
  }

  if (bounds().IsEmpty()) {
    views::View::Layout();
    return;
  }

  if (scroller_) {
    scroller_->set_fixed_size(gfx::Size());
    gfx::Size size = GetPreferredSize();

    // Set the scroller to fill the space above the bottom row, so that the
    // bottom row of the detailed view will always stay just above the title
    // row.
    gfx::Size scroller_size = scroll_content_->GetPreferredSize();
    scroller_->set_fixed_size(
        gfx::Size(width() + scroller_->GetScrollBarWidth(),
                  scroller_size.height() - (size.height() - height())));
  }

  views::View::Layout();

  if (title_row_) {
    // Always make sure the title row is bottom-aligned in non-MD.
    gfx::Rect fbounds = title_row_->bounds();
    fbounds.set_y(height() - title_row_->height());
    title_row_->SetBoundsRect(fbounds);
  }
}

int TrayDetailsView::GetHeightForWidth(int width) const {
  if (!UseMd() || bounds().IsEmpty())
    return views::View::GetHeightForWidth(width);

  // The height of the bubble that contains this detailed view is set to
  // the preferred height of the default view, and that determines the
  // initial height of |this|. Always request to stay the same height.
  return height();
}

void TrayDetailsView::OnPaintBorder(gfx::Canvas* canvas) {
  if (scroll_border_) {
    int index = GetIndexOf(scroller_);
    if (index < child_count() - 1 && child_at(index + 1) != title_row_)
      scroll_border_->set_visible(true);
    else
      scroll_border_->set_visible(false);
  }

  views::View::OnPaintBorder(canvas);
}

}  // namespace ash
