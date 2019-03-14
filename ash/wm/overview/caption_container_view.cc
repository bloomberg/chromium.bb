// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/caption_container_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize{24, 24};

constexpr int kCloseButtonInkDropInsetDp = 2;

constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

// The colors of the close button ripple.
constexpr SkColor kCloseButtonInkDropRippleColor =
    SkColorSetA(kCloseButtonColor, 0x0F);
constexpr SkColor kCloseButtonInkDropRippleHighlightColor =
    SkColorSetA(kCloseButtonColor, 0x14);

// The font delta of the overview window title.
constexpr int kLabelFontDelta = 2;

// Values of the backdrop.
constexpr int kBackdropRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

void AddChildWithLayer(views::View* parent, views::View* child) {
  child->SetPaintToLayer();
  child->layer()->SetFillsBoundsOpaquely(false);
  parent->AddChildView(child);
}

gfx::PointF ConvertToScreen(views::View* view, const gfx::Point& location) {
  gfx::Point location_copy(location);
  views::View::ConvertPointToScreen(view, &location_copy);
  return gfx::PointF(location_copy);
}

}  // namespace

// The close button for the caption container view. It has a custom ink drop.
class CaptionContainerView::OverviewCloseButton : public views::ImageButton {
 public:
  explicit OverviewCloseButton(
      CaptionContainerView::EventDelegate* event_delegate)
      : views::ImageButton(nullptr), event_delegate_(event_delegate) {
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kOverviewWindowCloseIcon, kCloseButtonColor));
    SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                      views::ImageButton::ALIGN_MIDDLE);
    SetMinimumImageSize(gfx::Size(kHeaderHeightDp, kHeaderHeightDp));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  }

  ~OverviewCloseButton() override = default;

  // Resets the listener so that the listener can go out of scope.
  void ResetEventDelegate() { event_delegate_ = nullptr; }

 protected:
  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
        kCloseButtonInkDropRippleColor, /*visible_opacity=*/1.f);
  }
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(
            kCloseButtonInkDropRippleHighlightColor, GetInkDropRadius()));
  }
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
  }
  void NotifyClick(const ui::Event& event) override {
    if (event_delegate_)
      event_delegate_->HandleCloseButtonClicked();
  }

 private:
  int GetInkDropRadius() const {
    return std::min(size().width(), size().height()) / 2 -
           kCloseButtonInkDropInsetDp;
  }

  CaptionContainerView::EventDelegate* event_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
};

CaptionContainerView::CaptionContainerView(EventDelegate* event_delegate,
                                           aura::Window* window)
    : Button(nullptr), event_delegate_(event_delegate) {
  // This should not be focusable. It's also to avoid accessibility error when
  // |window->GetTitle()| is empty.
  SetFocusBehavior(FocusBehavior::NEVER);
  SetAccessibleName(window->GetTitle());

  header_view_ = new views::View();
  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          kHorizontalLabelPaddingDp));
  AddChildWithLayer(this, header_view_);

  // Prefer kAppIconSmallKey (set by the client in Mash), then kAppIconKey and
  // kWindowIconKey (set for client windows in classic Ash but not Mash).
  gfx::ImageSkia* icon = window->GetProperty(aura::client::kAppIconSmallKey);
  if (!icon || icon->size().IsEmpty())
    icon = window->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = window->GetProperty(aura::client::kWindowIconKey);
  if (icon && !icon->size().IsEmpty()) {
    image_view_ = new views::ImageView();
    image_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
    image_view_->SetSize(kIconSize);
    header_view_->AddChildView(image_view_);
  }

  title_label_ = new views::Label(window->GetTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kLabelColor);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header_view_->AddChildView(title_label_);
  layout->SetFlexForView(title_label_, 1);

  close_button_ = new OverviewCloseButton(event_delegate);
  AddChildWithLayer(header_view_, close_button_);
}

CaptionContainerView::~CaptionContainerView() = default;

RoundedRectView* CaptionContainerView::GetCannotSnapContainer() {
  if (!cannot_snap_container_) {
    cannot_snap_label_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP));
    cannot_snap_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    cannot_snap_label_->SetAutoColorReadabilityEnabled(false);
    cannot_snap_label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    cannot_snap_label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

    cannot_snap_container_ = new RoundedRectView(
        kSplitviewLabelRoundRectRadiusDp, kSplitviewLabelBackgroundColor);
    cannot_snap_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    cannot_snap_container_->AddChildView(cannot_snap_label_);
    cannot_snap_container_->set_can_process_events_within_subtree(false);
    AddChildWithLayer(this, cannot_snap_container_);
    cannot_snap_container_->layer()->SetOpacity(0.f);
    Layout();
  }
  return cannot_snap_container_;
}

void CaptionContainerView::SetHeaderVisibility(HeaderVisibility visibility) {
  DCHECK(close_button_->layer());
  DCHECK(header_view_->layer());

  // Make the close button invisible if the rest of the header is to be shown.
  // If the rest of the header is to be hidden, make the close button visible
  // as |header_view_|'s opacity will be 0.f, hiding the close button. Modify
  // |close_button_|'s opacity instead of visibilty so the flex from its
  // sibling views do not mess up its layout.
  close_button_->layer()->SetOpacity(
      visibility == HeaderVisibility::kCloseButtonInvisibleOnly ? 0.f : 1.f);
  const bool visible = visibility != HeaderVisibility::kInvisible;
  AnimateLayerOpacity(header_view_->layer(), visible);
}

void CaptionContainerView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible)
    return;

  if (!backdrop_view_) {
    backdrop_view_ = new RoundedRectView(kBackdropRoundingDp, kBackdropColor);
    backdrop_view_->set_can_process_events_within_subtree(false);
    AddChildWithLayer(this, backdrop_view_);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void CaptionContainerView::SetCannotSnapLabelVisibility(bool visible) {
  if (!cannot_snap_container_ && !visible)
    return;

  DoSplitviewOpacityAnimation(GetCannotSnapContainer()->layer(),
                              visible
                                  ? SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN
                                  : SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
}

void CaptionContainerView::ResetEventDelegate() {
  event_delegate_ = nullptr;
  close_button_->ResetEventDelegate();
}

void CaptionContainerView::SetTitle(const base::string16& title) {
  title_label_->SetText(title);
  SetAccessibleName(title);
}

views::ImageButton* CaptionContainerView::GetCloseButton() {
  return close_button_;
}

void CaptionContainerView::Layout() {
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(kOverviewMargin, kOverviewMargin);

  const int visible_height = close_button_->GetPreferredSize().height();
  if (backdrop_view_) {
    gfx::Rect backdrop_bounds = bounds;
    backdrop_bounds.Inset(0, visible_height, 0, 0);
    backdrop_view_->SetBoundsRect(backdrop_bounds);
  }

  if (cannot_snap_container_) {
    gfx::Size label_size = cannot_snap_label_->CalculatePreferredSize();
    label_size.set_width(
        std::min(label_size.width() + 2 * kSplitviewLabelHorizontalInsetDp,
                 bounds.width() - 2 * kSplitviewLabelHorizontalInsetDp));
    label_size.set_height(
        std::max(label_size.height(), kSplitviewLabelPreferredHeightDp));

    // Position the cannot snap label in the middle of the item, minus the
    // title.
    gfx::Rect cannot_snap_bounds = GetLocalBounds();
    cannot_snap_bounds.Inset(0, visible_height, 0, 0);
    cannot_snap_bounds.ClampToCenteredSize(label_size);
    cannot_snap_container_->SetBoundsRect(cannot_snap_bounds);
  }

  // Position the header at the top.
  const gfx::Rect header_bounds(kOverviewMargin, kOverviewMargin,
                                GetLocalBounds().width() - kOverviewMargin,
                                visible_height);
  header_view_->SetBoundsRect(header_bounds);
}

const char* CaptionContainerView::GetClassName() const {
  return "CaptionContainerView";
}

bool CaptionContainerView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMousePressed(event);
  event_delegate_->HandlePressEvent(ConvertToScreen(this, event.location()));
  return true;
}

bool CaptionContainerView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event_delegate_)
    return Button::OnMouseDragged(event);
  event_delegate_->HandleDragEvent(ConvertToScreen(this, event.location()));
  return true;
}

void CaptionContainerView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event_delegate_) {
    Button::OnMouseReleased(event);
    return;
  }
  event_delegate_->HandleReleaseEvent(ConvertToScreen(this, event.location()));
}

void CaptionContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (!event_delegate_)
    return;

  if (event_delegate_->ShouldIgnoreGestureEvents()) {
    event->SetHandled();
    return;
  }

  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      event_delegate_->HandlePressEvent(location);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      event_delegate_->HandleDragEvent(location);
      break;
    case ui::ET_SCROLL_FLING_START:
      event_delegate_->HandleFlingStartEvent(location,
                                             event->details().velocity_x(),
                                             event->details().velocity_y());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      event_delegate_->HandleReleaseEvent(location);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      event_delegate_->HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      event_delegate_->HandleTapEvent();
      break;
    case ui::ET_GESTURE_END:
      event_delegate_->HandleGestureEndEvent();
      break;
    default:
      break;
  }
  event->SetHandled();
}

bool CaptionContainerView::CanAcceptEvent(const ui::Event& event) {
  bool accept_events = true;
  // Do not process or accept press down events that are on the border.
  static ui::EventType press_types[] = {ui::ET_GESTURE_TAP_DOWN,
                                        ui::ET_MOUSE_PRESSED};
  if (event.IsLocatedEvent() &&
      base::ContainsValue(press_types, event.type())) {
    gfx::Rect inset_bounds = GetLocalBounds();
    inset_bounds.Inset(gfx::Insets(kOverviewMargin));
    if (!inset_bounds.Contains(event.AsLocatedEvent()->location()))
      accept_events = false;
  }

  return accept_events && Button::CanAcceptEvent(event);
}

void CaptionContainerView::AnimateLayerOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  layer->SetOpacity(1.f - target_opacity);
  ScopedOverviewAnimationSettings settings(
      visible ? OVERVIEW_ANIMATION_OVERVIEW_TITLE_FADE_IN
              : OVERVIEW_ANIMATION_OVERVIEW_TITLE_FADE_OUT,
      layer->GetAnimator());
  layer->SetOpacity(target_opacity);
}

}  // namespace ash
