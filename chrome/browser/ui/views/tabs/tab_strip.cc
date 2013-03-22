// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#if defined(OS_WIN)
#include <windowsx.h>
#endif

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/default_theme_provider.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_util.h"
#include "ui/views/widget/monitor_win.h"
#include "win8/util/win8_util.h"
#endif

using content::UserMetricsAction;
using ui::DropTargetEvent;

namespace {

static const int kTabStripAnimationVSlop = 40;
// Inactive tabs in a native frame are slightly transparent.
static const int kNativeFrameInactiveTabAlpha = 200;
// If there are multiple tabs selected then make non-selected inactive tabs
// even more transparent.
static const int kNativeFrameInactiveTabAlphaMultiSelection = 150;

// Alpha applied to all elements save the selected tabs.
static const int kInactiveTabAndNewTabButtonAlphaAsh = 230;
static const int kInactiveTabAndNewTabButtonAlpha = 255;

// Inverse ratio of the width of a tab edge to the width of the tab. When
// hovering over the left or right edge of a tab, the drop indicator will
// point between tabs.
static const int kTabEdgeRatioInverse = 4;

// Size of the drop indicator.
static int drop_indicator_width;
static int drop_indicator_height;

static inline int Round(double x) {
  // Why oh why is this not in a standard header?
  return static_cast<int>(floor(x + 0.5));
}

// Max number of stacked tabs.
static const int kMaxStackedCount = 4;

// Padding between stacked tabs.
static const int kStackedPadding = 6;

// See UpdateLayoutTypeFromMouseEvent() for a description of these.
const int kMouseMoveTimeMS = 200;
const int kMouseMoveCountBeforeConsiderReal = 3;

// Amount of time we delay before resizing after a close from a touch.
const int kTouchResizeLayoutTimeMS = 2000;

// Horizontal offset for the new tab button to bring it closer to the
// rightmost tab.
int newtab_button_h_offset() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = -11;
        break;
      case ui::LAYOUT_TOUCH:
        value = -13;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Vertical offset for the new tab button to bring it closer to the
// rightmost tab.
int newtab_button_v_offset() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = 7;
        break;
      case ui::LAYOUT_TOUCH:
        value = 8;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Amount the left edge of a tab is offset from the rectangle of the tab's
// favicon/title/close box.  Related to the width of IDR_TAB_ACTIVE_LEFT.
// Affects the size of the "V" between adjacent tabs.
int tab_h_offset() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = -26;
        break;
      case ui::LAYOUT_TOUCH:
        value = -39;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// The size of the new tab button must be hardcoded because we need to be
// able to lay it out before we are able to get its image from the
// ui::ThemeProvider.  It also makes sense to do this, because the size of the
// new tab button should not need to be calculated dynamically.
int newtab_button_asset_width() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = 34;
        break;
      case ui::LAYOUT_TOUCH:
        value = 46;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int newtab_button_asset_height() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = 18;
        break;
      case ui::LAYOUT_TOUCH:
        value = 24;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Amount to adjust the clip by when the tab is stacked before the active index.
int stacked_tab_left_clip() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = 20;
        break;
      case ui::LAYOUT_TOUCH:
        value = 26;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Amount to adjust the clip by when the tab is stacked after the active index.
int stacked_tab_right_clip() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_DESKTOP:
        value = 20;
        break;
      case ui::LAYOUT_TOUCH:
        value = 26;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit ResetDraggingStateDelegate(Tab* tab) : tab_(tab) {
  }

  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    tab_->set_dragging(false);
  }

  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE {
    tab_->set_dragging(false);
  }

 private:
  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(ResetDraggingStateDelegate);
};

// If |dest| contains the point |point_in_source| the event handler from |dest|
// is returned. Otherwise NULL is returned.
views::View* ConvertPointToViewAndGetEventHandler(
    views::View* source,
    views::View* dest,
    const gfx::Point& point_in_source) {
  gfx::Point dest_point(point_in_source);
  views::View::ConvertPointToTarget(source, dest, &dest_point);
  return dest->HitTestPoint(dest_point) ?
      dest->GetEventHandlerForPoint(dest_point) : NULL;
}

TabDragController::EventSource EventSourceFromEvent(
    const ui::LocatedEvent& event) {
  return event.IsGestureEvent() ? TabDragController::EVENT_SOURCE_TOUCH :
      TabDragController::EVENT_SOURCE_MOUSE;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabButton
//
//  A subclass of button that hit-tests to the shape of the new tab button and
//  does custom drawing.

class NewTabButton : public views::ImageButton {
 public:
  NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener);
  virtual ~NewTabButton();

  // Set the background offset used to match the background image to the frame
  // image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

 protected:
  // Overridden from views::View:
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* path) const OVERRIDE;
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
#endif
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  bool ShouldUseNativeFrame() const;
  gfx::ImageSkia GetBackgroundImage(views::CustomButton::ButtonState state,
                                    ui::ScaleFactor scale_factor) const;
  gfx::ImageSkia GetImageForState(views::CustomButton::ButtonState state,
                                  ui::ScaleFactor scale_factor) const;
  gfx::ImageSkia GetImageForScale(ui::ScaleFactor scale_factor) const;

  // Tab strip that contains this button.
  TabStrip* tab_strip_;

  // The offset used to paint the background image.
  gfx::Point background_offset_;

  DISALLOW_COPY_AND_ASSIGN(NewTabButton);
};

NewTabButton::NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener)
    : views::ImageButton(listener),
      tab_strip_(tab_strip) {
}

NewTabButton::~NewTabButton() {
}

bool NewTabButton::HasHitTestMask() const {
  // When the button is sized to the top of the tab strip we want the user to
  // be able to click on complete bounds, and so don't return a custom hit
  // mask.
  return !tab_strip_->SizeTabButtonToTopOfTabStrip();
}

void NewTabButton::GetHitTestMask(gfx::Path* path) const {
  DCHECK(path);

  SkScalar w = SkIntToScalar(width());
  SkScalar v_offset = SkIntToScalar(newtab_button_v_offset());

  // These values are defined by the shape of the new tab image. Should that
  // image ever change, these values will need to be updated. They're so
  // custom it's not really worth defining constants for.
  // These values are correct for regular and USE_ASH versions of the image.
  path->moveTo(0, v_offset + 1);
  path->lineTo(w - 7, v_offset + 1);
  path->lineTo(w - 4, v_offset + 4);
  path->lineTo(w, v_offset + 16);
  path->lineTo(w - 1, v_offset + 17);
  path->lineTo(7, v_offset + 17);
  path->lineTo(4, v_offset + 13);
  path->lineTo(0, v_offset + 1);
  path->close();
}

#if defined(OS_WIN) && !defined(USE_AURA)
void NewTabButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyRightMouseButton()) {
    gfx::Point point(event.x(), event.y());
    views::View::ConvertPointToScreen(this, &point);
    ui::ShowSystemMenu(GetWidget()->GetNativeView(), point.x(), point.y());
    SetState(views::CustomButton::STATE_NORMAL);
    return;
  }
  views::ImageButton::OnMouseReleased(event);
}
#endif

void NewTabButton::OnPaint(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetImageForScale(canvas->scale_factor());
  canvas->DrawImageInt(image, 0, height() - image.height());
}

void NewTabButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  views::ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

bool NewTabButton::ShouldUseNativeFrame() const {
  return GetWidget() &&
    GetWidget()->GetTopLevelWidget()->ShouldUseNativeFrame();
}

gfx::ImageSkia NewTabButton::GetBackgroundImage(
    views::CustomButton::ButtonState state,
    ui::ScaleFactor scale_factor) const {
  int background_id = 0;
  if (ShouldUseNativeFrame()) {
    background_id = IDR_THEME_TAB_BACKGROUND_V;
  } else if (tab_strip_->controller()->IsIncognito()) {
    background_id = IDR_THEME_TAB_BACKGROUND_INCOGNITO;
#if defined(OS_WIN)
  } else if (win8::IsSingleWindowMetroMode()) {
    background_id = IDR_THEME_TAB_BACKGROUND_V;
#endif
  } else {
    background_id = IDR_THEME_TAB_BACKGROUND;
  }

  int alpha = 0;
  switch (state) {
    case views::CustomButton::STATE_NORMAL:
    case views::CustomButton::STATE_HOVERED:
      alpha = ShouldUseNativeFrame() ? kNativeFrameInactiveTabAlpha : 255;
      break;
    case views::CustomButton::STATE_PRESSED:
      alpha = 145;
      break;
    default:
      NOTREACHED();
      break;
  }

  gfx::ImageSkia* mask =
      GetThemeProvider()->GetImageSkiaNamed(IDR_NEWTAB_BUTTON_MASK);
  int height = mask->height();
  int width = mask->width();

  // The canvas and mask has to use the same scale factor.
  if (!mask->HasRepresentation(scale_factor))
    scale_factor = ui::SCALE_FACTOR_100P;

  gfx::Canvas canvas(gfx::Size(width, height), scale_factor, false);

  // For custom images the background starts at the top of the tab strip.
  // Otherwise the background starts at the top of the frame.
  gfx::ImageSkia* background =
      GetThemeProvider()->GetImageSkiaNamed(background_id);
  int offset_y = GetThemeProvider()->HasCustomImage(background_id) ?
      0 : background_offset_.y();

  // The new tab background is mirrored in RTL mode, but the theme background
  // should never be mirrored. Mirror it here to compensate.
  float x_scale = 1.0f;
  int x = GetMirroredX() + background_offset_.x();
  if (base::i18n::IsRTL()) {
    x_scale = -1.0f;
    // Offset by |width| such that the same region is painted as if there was no
    // flip.
    x += width;
  }
  canvas.TileImageInt(*background, x, newtab_button_v_offset() + offset_y,
                      x_scale, 1.0f, 0, 0, width, height);

  if (alpha != 255) {
    SkPaint paint;
    paint.setColor(SkColorSetARGB(alpha, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    canvas.DrawRect(gfx::Rect(0, 0, width, height), paint);
  }

  // White highlight on hover.
  if (state == views::CustomButton::STATE_HOVERED)
    canvas.FillRect(GetLocalBounds(), SkColorSetARGB(64, 255, 255, 255));

  return gfx::ImageSkiaOperations::CreateMaskedImage(
      gfx::ImageSkia(canvas.ExtractImageRep()), *mask);
}

gfx::ImageSkia NewTabButton::GetImageForState(
    views::CustomButton::ButtonState state,
    ui::ScaleFactor scale_factor) const {
  const int overlay_id = state == views::CustomButton::STATE_PRESSED ?
        IDR_NEWTAB_BUTTON_P : IDR_NEWTAB_BUTTON;
  gfx::ImageSkia* overlay = GetThemeProvider()->GetImageSkiaNamed(overlay_id);

  gfx::Canvas canvas(
      gfx::Size(overlay->width(), overlay->height()), scale_factor, false);
  canvas.DrawImageInt(GetBackgroundImage(state, scale_factor), 0, 0);

  // Draw the button border with a slight alpha.
  const int kNativeFrameOverlayAlpha = 178;
  const int kOpaqueFrameOverlayAlpha = 230;
  uint8 alpha = ShouldUseNativeFrame() ?
      kNativeFrameOverlayAlpha : kOpaqueFrameOverlayAlpha;
  canvas.DrawImageInt(*overlay, 0, 0, alpha);

  return gfx::ImageSkia(canvas.ExtractImageRep());
}

gfx::ImageSkia NewTabButton::GetImageForScale(
    ui::ScaleFactor scale_factor) const {
  if (!hover_animation_->is_animating())
    return GetImageForState(state(), scale_factor);
  return gfx::ImageSkiaOperations::CreateBlendedImage(
      GetImageForState(views::CustomButton::STATE_NORMAL, scale_factor),
      GetImageForState(views::CustomButton::STATE_HOVERED, scale_factor),
      hover_animation_->GetCurrentValue());
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabStrip::RemoveTabDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  RemoveTabDelegate(TabStrip* tab_strip, Tab* tab);

  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

 private:
  void CompleteRemove();

  // When the animation completes, we send the Container a message to simulate
  // a mouse moved event at the current mouse position. This tickles the Tab
  // the mouse is currently over to show the "hot" state of the close button.
  void HighlightCloseButton();

  TabStrip* tabstrip_;
  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(RemoveTabDelegate);
};

TabStrip::RemoveTabDelegate::RemoveTabDelegate(TabStrip* tab_strip,
                                               Tab* tab)
    : tabstrip_(tab_strip),
      tab_(tab) {
}

void TabStrip::RemoveTabDelegate::AnimationEnded(
    const ui::Animation* animation) {
  CompleteRemove();
}

void TabStrip::RemoveTabDelegate::AnimationCanceled(
    const ui::Animation* animation) {
  CompleteRemove();
}

void TabStrip::RemoveTabDelegate::CompleteRemove() {
  DCHECK(tab_->closing());
  tabstrip_->RemoveAndDeleteTab(tab_);
  HighlightCloseButton();
}

void TabStrip::RemoveTabDelegate::HighlightCloseButton() {
  if (tabstrip_->IsDragSessionActive() ||
      !tabstrip_->ShouldHighlightCloseButtonAfterRemove()) {
    // This function is not required (and indeed may crash!) for removes
    // spawned by non-mouse closes and drag-detaches.
    return;
  }

  views::Widget* widget = tabstrip_->GetWidget();
  // This can be null during shutdown. See http://crbug.com/42737.
  if (!widget)
    return;

  widget->ResetLastMouseMoveFlag();
  gfx::Point position = gfx::Screen::GetScreenFor(
      widget->GetNativeView())->GetCursorScreenPoint();
  views::View* root_view = widget->GetRootView();
  views::View::ConvertPointFromScreen(root_view, &position);
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED,
                             position, position,
                             ui::EF_IS_SYNTHESIZED);
  root_view->OnMouseMoved(mouse_event);
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, public:

// static
const char TabStrip::kViewClassName[] = "TabStrip";

// static
const int TabStrip::kMiniToNonMiniGap = 3;

TabStrip::TabStrip(TabStripController* controller)
    : controller_(controller),
      newtab_button_(NULL),
      current_unselected_width_(Tab::GetStandardSize().width()),
      current_selected_width_(Tab::GetStandardSize().width()),
      available_width_for_tabs_(-1),
      in_tab_close_(false),
      animation_container_(new ui::AnimationContainer()),
      ALLOW_THIS_IN_INITIALIZER_LIST(bounds_animator_(this)),
      layout_type_(TAB_STRIP_LAYOUT_SHRINK),
      adjust_layout_(false),
      reset_to_shrink_on_exit_(false),
      mouse_move_count_(0),
      immersive_style_(false) {
  Init();
}

TabStrip::~TabStrip() {
  FOR_EACH_OBSERVER(TabStripObserver, observers_,
                    TabStripDeleted(this));

  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  DestroyDragController();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  // The children (tabs) may callback to us from their destructor. Delete them
  // so that if they call back we aren't in a weird state.
  RemoveAllChildViews(true);
}

void TabStrip::AddObserver(TabStripObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStrip::RemoveObserver(TabStripObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabStrip::SetLayoutType(TabStripLayoutType layout_type,
                             bool adjust_layout) {
  adjust_layout_ = adjust_layout;
  if (layout_type == layout_type_)
    return;

  const int active_index = controller_->GetActiveIndex();
  int active_center = 0;
  if (active_index != -1) {
    active_center = ideal_bounds(active_index).x() +
        ideal_bounds(active_index).width() / 2;
  }
  layout_type_ = layout_type;
  SetResetToShrinkOnExit(false);
  SwapLayoutIfNecessary();
  // When transitioning to stacked try to keep the active tab centered.
  if (touch_layout_.get() && active_index != -1) {
    touch_layout_->SetActiveTabLocation(
        active_center - ideal_bounds(active_index).width() / 2);
    AnimateToIdealBounds();
  }
}

gfx::Rect TabStrip::GetNewTabButtonBounds() {
  return newtab_button_->bounds();
}

bool TabStrip::SizeTabButtonToTopOfTabStrip() {
  // Extend the button to the screen edge in maximized and immersive fullscreen.
  views::Widget* widget = GetWidget();
  return browser_defaults::kSizeTabButtonToTopOfTabStrip ||
      (widget && (widget->IsMaximized() || widget->IsFullscreen()));
}

void TabStrip::StartHighlight(int model_index) {
  tab_at(model_index)->StartPulse();
}

void TabStrip::StopAllHighlighting() {
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->StopPulse();
}

void TabStrip::AddTabAt(int model_index,
                        const TabRendererData& data,
                        bool is_active) {
  // Stop dragging when a new tab is added and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the add,
  // but this comes up infrequently enough that it's not work the complexity.
  if (drag_controller_.get() && !drag_controller_->is_mutating() &&
      drag_controller_->is_dragging_window()) {
    EndDrag(END_DRAG_COMPLETE);
  }
  Tab* tab = CreateTab();
  tab->SetData(data);
  UpdateTabsClosingMap(model_index, 1);
  tabs_.Add(tab, model_index);
  AddChildView(tab);

  if (touch_layout_.get()) {
    GenerateIdealBoundsForMiniTabs(NULL);
    int add_types = 0;
    if (data.mini)
      add_types |= StackedTabStripLayout::kAddTypeMini;
    if (is_active)
      add_types |= StackedTabStripLayout::kAddTypeActive;
    touch_layout_->AddTab(model_index, add_types, GetStartXForNormalTabs());
  }

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (tab_count() > 1 && GetWidget() && GetWidget()->IsVisible())
    StartInsertTabAnimation(model_index);
  else
    DoLayout();

  SwapLayoutIfNecessary();

  FOR_EACH_OBSERVER(TabStripObserver, observers_,
                    TabStripAddedTabAt(this, model_index));
}

void TabStrip::MoveTab(int from_model_index,
                       int to_model_index,
                       const TabRendererData& data) {
  DCHECK_GT(tabs_.view_size(), 0);
  Tab* last_tab = tab_at(tab_count() - 1);
  tab_at(from_model_index)->SetData(data);
  if (touch_layout_.get()) {
    tabs_.MoveViewOnly(from_model_index, to_model_index);
    int mini_count = 0;
    GenerateIdealBoundsForMiniTabs(&mini_count);
    touch_layout_->MoveTab(
        from_model_index, to_model_index, controller_->GetActiveIndex(),
        GetStartXForNormalTabs(), mini_count);
  } else {
    tabs_.Move(from_model_index, to_model_index);
  }
  StartMoveTabAnimation();
  if (TabDragController::IsAttachedTo(this) &&
      (last_tab != tab_at(tab_count() - 1) || last_tab->dragging())) {
    newtab_button_->SetVisible(false);
  }
  SwapLayoutIfNecessary();

  FOR_EACH_OBSERVER(TabStripObserver, observers_,
                    TabStripMovedTab(this, from_model_index, to_model_index));
}

void TabStrip::RemoveTabAt(int model_index) {
  if (touch_layout_.get()) {
    Tab* tab = tab_at(model_index);
    tab->set_closing(true);
    int old_x = tabs_.ideal_bounds(model_index).x();
    // We still need to paint the tab until we actually remove it. Put it in
    // tabs_closing_map_ so we can find it.
    RemoveTabFromViewModel(model_index);
    touch_layout_->RemoveTab(model_index, GenerateIdealBoundsForMiniTabs(NULL),
                             old_x);
    ScheduleRemoveTabAnimation(tab);
  } else if (in_tab_close_ && model_index != GetModelCount()) {
    StartMouseInitiatedRemoveTabAnimation(model_index);
  } else {
    StartRemoveTabAnimation(model_index);
  }
  SwapLayoutIfNecessary();

  FOR_EACH_OBSERVER(TabStripObserver, observers_,
                    TabStripRemovedTabAt(this, model_index));
}

void TabStrip::SetTabData(int model_index, const TabRendererData& data) {
  Tab* tab = tab_at(model_index);
  bool mini_state_changed = tab->data().mini != data.mini;
  tab->SetData(data);

  if (mini_state_changed) {
    if (touch_layout_.get()) {
      int mini_tab_count = 0;
      int start_x = GenerateIdealBoundsForMiniTabs(&mini_tab_count);
      touch_layout_->SetXAndMiniCount(start_x, mini_tab_count);
    }
    if (GetWidget() && GetWidget()->IsVisible())
      StartMiniTabAnimation();
    else
      DoLayout();
  }
  SwapLayoutIfNecessary();
}

void TabStrip::PrepareForCloseAt(int model_index, CloseTabSource source) {
  if (!in_tab_close_ && IsAnimating()) {
    // Cancel any current animations. We do this as remove uses the current
    // ideal bounds and we need to know ideal bounds is in a good state.
    StopAnimating(true);
  }

  if (!GetWidget())
    return;

  int model_count = GetModelCount();
  if (model_index + 1 != model_count && model_count > 1) {
    // The user is about to close a tab other than the last tab. Set
    // available_width_for_tabs_ so that if we do a layout we don't position a
    // tab past the end of the second to last tab. We do this so that as the
    // user closes tabs with the mouse a tab continues to fall under the mouse.
    Tab* last_tab = tab_at(model_count - 1);
    Tab* tab_being_removed = tab_at(model_index);
    available_width_for_tabs_ = last_tab->x() + last_tab->width() -
        tab_being_removed->width() - tab_h_offset();
    if (model_index == 0 && tab_being_removed->data().mini &&
        !tab_at(1)->data().mini) {
      available_width_for_tabs_ -= kMiniToNonMiniGap;
    }
  }

  in_tab_close_ = true;
  resize_layout_timer_.Stop();
  if (source == CLOSE_TAB_FROM_TOUCH) {
    StartResizeLayoutTabsFromTouchTimer();
  } else {
    AddMessageLoopObserver();
  }
}

void TabStrip::SetSelection(const ui::ListSelectionModel& old_selection,
                            const ui::ListSelectionModel& new_selection) {
  if (touch_layout_.get()) {
    touch_layout_->SetActiveIndex(new_selection.active());
    // Only start an animation if we need to. Otherwise clicking on an
    // unselected tab and dragging won't work because dragging is only allowed
    // if not animating.
    if (!views::ViewModelUtils::IsAtIdealBounds(tabs_))
      AnimateToIdealBounds();
    SchedulePaint();
  } else {
    // We have "tiny tabs" if the tabs are so tiny that the unselected ones are
    // a different size to the selected ones.
    bool tiny_tabs = current_unselected_width_ != current_selected_width_;
    if (!IsAnimating() && (!in_tab_close_ || tiny_tabs)) {
      DoLayout();
    } else {
      SchedulePaint();
    }
  }

  ui::ListSelectionModel::SelectedIndices no_longer_selected;
  std::insert_iterator<ui::ListSelectionModel::SelectedIndices>
      it(no_longer_selected, no_longer_selected.begin());
  std::set_difference(old_selection.selected_indices().begin(),
                      old_selection.selected_indices().end(),
                      new_selection.selected_indices().begin(),
                      new_selection.selected_indices().end(),
                      it);
  for (size_t i = 0; i < no_longer_selected.size(); ++i)
    tab_at(no_longer_selected[i])->StopMiniTabTitleAnimation();
}

void TabStrip::TabTitleChangedNotLoading(int model_index) {
  Tab* tab = tab_at(model_index);
  if (tab->data().mini && !tab->IsActive())
    tab->StartMiniTabTitleAnimation();
}

Tab* TabStrip::tab_at(int index) const {
  return static_cast<Tab*>(tabs_.view_at(index));
}

int TabStrip::GetModelIndexOfTab(const Tab* tab) const {
  return tabs_.GetIndexOfView(tab);
}

int TabStrip::GetModelCount() const {
  return controller_->GetCount();
}

bool TabStrip::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

Tab* TabStrip::CreateTabForDragging() {
  Tab* tab = new Tab(NULL);
  // Make sure the dragged tab shares our theme provider. We need to explicitly
  // do this as during dragging there isn't a theme provider.
  tab->set_theme_provider(GetThemeProvider());
  return tab;
}

bool TabStrip::IsDragSessionActive() const {
  return drag_controller_.get() != NULL;
}

bool TabStrip::IsActiveDropTarget() const {
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    if (tab->dragging())
      return true;
  }
  return false;
}

bool TabStrip::IsTabStripEditable() const {
  return !IsDragSessionActive() && !IsActiveDropTarget();
}

bool TabStrip::IsTabStripCloseable() const {
  return !IsDragSessionActive();
}

void TabStrip::UpdateLoadingAnimations() {
  controller_->UpdateLoadingAnimations();
}

bool TabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  views::View* v = GetEventHandlerForPoint(point);

  // If there is no control at this location, claim the hit was in the title
  // bar to get a move action.
  if (v == this)
    return true;

  // Check to see if the point is within the non-button parts of the new tab
  // button. The button has a non-rectangular shape, so if it's not in the
  // visual portions of the button we treat it as a click to the caption.
  gfx::Point point_in_newtab_coords(point);
  View::ConvertPointToTarget(this, newtab_button_, &point_in_newtab_coords);
  if (newtab_button_->GetLocalBounds().Contains(point_in_newtab_coords) &&
      !newtab_button_->HitTestPoint(point_in_newtab_coords)) {
    return true;
  }

  // All other regions, including the new Tab button, should be considered part
  // of the containing Window's client area so that regular events can be
  // processed for them.
  return false;
}

void TabStrip::SetBackgroundOffset(const gfx::Point& offset) {
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->set_background_offset(offset);
  newtab_button_->set_background_offset(offset);
}

views::View* TabStrip::newtab_button() {
  return newtab_button_;
}

void TabStrip::SetImmersiveStyle(bool enable) {
  if (immersive_style_ == enable)
    return;
  immersive_style_ = enable;
}

bool TabStrip::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

void TabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator_.Cancel();

  if (layout)
    DoLayout();
}

const ui::ListSelectionModel& TabStrip::GetSelectionModel() {
  return controller_->GetSelectionModel();
}

bool TabStrip::SupportsMultipleSelection() {
  // TODO: currently only allow single selection in touch layout mode.
  return touch_layout_.get() == NULL;
}

void TabStrip::SelectTab(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->SelectTab(model_index);
}

void TabStrip::ExtendSelectionTo(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->ExtendSelectionTo(model_index);
}

void TabStrip::ToggleSelected(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->ToggleSelected(model_index);
}

void TabStrip::AddSelectionFromAnchorTo(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->AddSelectionFromAnchorTo(model_index);
}

void TabStrip::CloseTab(Tab* tab, CloseTabSource source) {
  if (tab->closing()) {
    // If the tab is already closing, close the next tab. We do this so that the
    // user can rapidly close tabs by clicking the close button and not have
    // the animations interfere with that.
    for (TabsClosingMap::const_iterator i(tabs_closing_map_.begin());
         i != tabs_closing_map_.end(); ++i) {
      std::vector<Tab*>::const_iterator j =
          std::find(i->second.begin(), i->second.end(), tab);
      if (j != i->second.end()) {
        if (i->first + 1 < GetModelCount())
          controller_->CloseTab(i->first + 1, source);
        return;
      }
    }
    // If we get here, it means a tab has been marked as closing but isn't in
    // the set of known closing tabs.
    NOTREACHED();
    return;
  }
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->CloseTab(model_index, source);
}

void TabStrip::ShowContextMenuForTab(Tab* tab, const gfx::Point& p) {
  controller_->ShowContextMenuForTab(tab, p);
}

bool TabStrip::IsActiveTab(const Tab* tab) const {
  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsActiveTab(model_index);
}

bool TabStrip::IsTabSelected(const Tab* tab) const {
  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabSelected(model_index);
}

bool TabStrip::IsTabPinned(const Tab* tab) const {
  if (tab->closing())
    return false;

  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabPinned(model_index);
}

void TabStrip::MaybeStartDrag(
    Tab* tab,
    const ui::LocatedEvent& event,
    const ui::ListSelectionModel& original_selection) {
  // Don't accidentally start any drag operations during animations if the
  // mouse is down... during an animation tabs are being resized automatically,
  // so the View system can misinterpret this easily if the mouse is down that
  // the user is dragging.
  if (IsAnimating() || tab->closing() ||
      controller_->HasAvailableDragActions() == 0) {
    return;
  }
  int model_index = GetModelIndexOfTab(tab);
  if (!IsValidModelIndex(model_index)) {
    CHECK(false);
    return;
  }
  std::vector<Tab*> tabs;
  int size_to_selected = 0;
  int x = tab->GetMirroredXInView(event.x());
  int y = event.y();
  // Build the set of selected tabs to drag and calculate the offset from the
  // first selected tab.
  for (int i = 0; i < tab_count(); ++i) {
    Tab* other_tab = tab_at(i);
    if (IsTabSelected(other_tab)) {
      tabs.push_back(other_tab);
      if (other_tab == tab) {
        size_to_selected = GetSizeNeededForTabs(tabs);
        x = size_to_selected - tab->width() + x;
      }
    }
  }
  DCHECK(!tabs.empty());
  DCHECK(std::find(tabs.begin(), tabs.end(), tab) != tabs.end());
  ui::ListSelectionModel selection_model;
  if (!original_selection.IsSelected(model_index))
    selection_model.Copy(original_selection);
  // Delete the existing DragController before creating a new one. We do this as
  // creating the DragController remembers the WebContents delegates and we need
  // to make sure the existing DragController isn't still a delegate.
  drag_controller_.reset();
  TabDragController::DetachBehavior detach_behavior =
      TabDragController::DETACHABLE;
  TabDragController::MoveBehavior move_behavior =
      TabDragController::REORDER;
  // Use MOVE_VISIBILE_TABS in the following conditions:
  // . Mouse event generated from touch and the left button is down (the right
  //   button corresponds to a long press, which we want to reorder).
  // . Gesture begin and control key isn't down.
  // . Real mouse event and control is down. This is mostly for testing.
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_GESTURE_BEGIN);
  if (touch_layout_.get() &&
      ((event.type() == ui::ET_MOUSE_PRESSED &&
        (((event.flags() & ui::EF_FROM_TOUCH) &&
          static_cast<const ui::MouseEvent&>(event).IsLeftMouseButton()) ||
         (!(event.flags() & ui::EF_FROM_TOUCH) &&
          static_cast<const ui::MouseEvent&>(event).IsControlDown()))) ||
       (event.type() == ui::ET_GESTURE_BEGIN && !event.IsControlDown()))) {
    move_behavior = TabDragController::MOVE_VISIBILE_TABS;
  }

  views::Widget* widget = GetWidget();

  // Don't allow detaching from maximized windows (in ash) when all the tabs are
  // selected and only one display. Since the window is maximized we know there
  // are no other tabbed browsers the user can drag to.
  const chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeView(widget->GetNativeView());
  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH &&
      widget->IsMaximized() &&
      static_cast<int>(tabs.size()) == tab_count() &&
      gfx::Screen::GetScreenFor(widget->GetNativeView())->GetNumDisplays() == 1)
    detach_behavior = TabDragController::NOT_DETACHABLE;

#if defined(OS_WIN)
  // It doesn't make sense to drag tabs out on Win8's single window Metro mode.
  if (win8::IsSingleWindowMetroMode())
    detach_behavior = TabDragController::NOT_DETACHABLE;
#endif
  // Gestures don't automatically do a capture. We don't allow multiple drags at
  // the same time, so we explicitly capture.
  if (event.type() == ui::ET_GESTURE_BEGIN)
    widget->SetCapture(this);
  drag_controller_.reset(new TabDragController);
  drag_controller_->Init(
      this, tab, tabs, gfx::Point(x, y), event.x(), selection_model,
      detach_behavior, move_behavior, EventSourceFromEvent(event));
}

void TabStrip::ContinueDrag(views::View* view, const ui::LocatedEvent& event) {
  if (drag_controller_.get() &&
      drag_controller_->event_source() == EventSourceFromEvent(event)) {
    gfx::Point screen_location(event.location());
    views::View::ConvertPointToScreen(view, &screen_location);
    drag_controller_->Drag(screen_location);
  }
}

bool TabStrip::EndDrag(EndDragReason reason) {
  if (!drag_controller_.get())
    return false;
  bool started_drag = drag_controller_->started_drag();
  drag_controller_->EndDrag(reason);
  return started_drag;
}

Tab* TabStrip::GetTabAt(Tab* tab, const gfx::Point& tab_in_tab_coordinates) {
  gfx::Point local_point = tab_in_tab_coordinates;
  ConvertPointToTarget(tab, this, &local_point);

  views::View* view = GetEventHandlerForPoint(local_point);
  if (!view)
    return NULL;  // No tab contains the point.

  // Walk up the view hierarchy until we find a tab, or the TabStrip.
  while (view && view != this && view->id() != VIEW_ID_TAB)
    view = view->parent();

  return view && view->id() == VIEW_ID_TAB ? static_cast<Tab*>(view) : NULL;
}

void TabStrip::OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) {
  UpdateLayoutTypeFromMouseEvent(source, event);
}

bool TabStrip::ShouldPaintTab(const Tab* tab, gfx::Rect* clip) {
  // Only touch layout needs to restrict the clip.
  if (!(touch_layout_.get() || IsStackingDraggedTabs()))
    return true;

  int index = GetModelIndexOfTab(tab);
  if (index == -1)
    return true;  // Tab is closing, paint it all.

  int active_index = IsStackingDraggedTabs() ?
      controller_->GetActiveIndex() : touch_layout_->active_index();
  if (active_index == tab_count())
    active_index--;

  if (index < active_index) {
    if (tab_at(index)->x() == tab_at(index + 1)->x())
      return false;

    if (tab_at(index)->x() > tab_at(index + 1)->x())
      return true;  // Can happen during dragging.

    clip->SetRect(0, 0, tab_at(index + 1)->x() - tab_at(index)->x() +
                      stacked_tab_left_clip(),
                  tab_at(index)->height());
  } else if (index > active_index && index > 0) {
    const gfx::Rect& tab_bounds(tab_at(index)->bounds());
    const gfx::Rect& previous_tab_bounds(tab_at(index - 1)->bounds());
    if (tab_bounds.x() == previous_tab_bounds.x())
      return false;

    if (tab_bounds.x() < previous_tab_bounds.x())
      return true;  // Can happen during dragging.

    if (previous_tab_bounds.right() + tab_h_offset() != tab_bounds.x()) {
      int x = previous_tab_bounds.right() - tab_bounds.x() -
          stacked_tab_right_clip();
      clip->SetRect(x, 0, tab_bounds.width() - x, tab_bounds.height());
    }
  }
  return true;
}

bool TabStrip::IsImmersiveStyle() const {
  return immersive_style_;
}

void TabStrip::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
  if (reset_to_shrink_on_exit_) {
    reset_to_shrink_on_exit_ = false;
    SetLayoutType(TAB_STRIP_LAYOUT_SHRINK, true);
    controller_->LayoutTypeMaybeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::View overrides:

void TabStrip::Layout() {
  // Only do a layout if our size changed.
  if (last_layout_size_ == size())
    return;
  if (IsDragSessionActive())
    return;
  DoLayout();
}

void TabStrip::PaintChildren(gfx::Canvas* canvas) {
  // The view order doesn't match the paint order (tabs_ contains the tab
  // ordering). Additionally we need to paint the tabs that are closing in
  // |tabs_closing_map_|.
  Tab* active_tab = NULL;
  std::vector<Tab*> tabs_dragging;
  std::vector<Tab*> selected_tabs;
  int selected_tab_count = 0;
  bool is_dragging = false;
  int active_tab_index = -1;
  // Since |touch_layout_| is created based on number of tabs and width we use
  // the ideal state to determine if we should paint stacked. This minimizes
  // painting changes as we switch between the two.
  const bool stacking = (layout_type_ == TAB_STRIP_LAYOUT_STACKED) ||
      IsStackingDraggedTabs();

  const chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeView(GetWidget()->GetNativeView());
  const int inactive_tab_alpha =
      host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH ?
      kInactiveTabAndNewTabButtonAlphaAsh :
      kInactiveTabAndNewTabButtonAlpha;

  if (inactive_tab_alpha < 255)
    canvas->SaveLayerAlpha(inactive_tab_alpha);

  PaintClosingTabs(canvas, tab_count());

  for (int i = tab_count() - 1; i >= 0; --i) {
    Tab* tab = tab_at(i);
    if (tab->IsSelected())
      selected_tab_count++;
    if (tab->dragging() && !stacking) {
      is_dragging = true;
      if (tab->IsActive()) {
        active_tab = tab;
        active_tab_index = i;
      } else {
        tabs_dragging.push_back(tab);
      }
    } else if (!tab->IsActive()) {
      if (!tab->IsSelected()) {
        if (!stacking)
          tab->Paint(canvas);
      } else {
        selected_tabs.push_back(tab);
      }
    } else {
      active_tab = tab;
      active_tab_index = i;
    }
    PaintClosingTabs(canvas, i);
  }

  // Draw from the left and then the right if we're in touch mode.
  if (stacking && active_tab_index >= 0) {
    for (int i = 0; i < active_tab_index; ++i) {
      Tab* tab = tab_at(i);
      tab->Paint(canvas);
    }

    for (int i = tab_count() - 1; i > active_tab_index; --i) {
      Tab* tab = tab_at(i);
      tab->Paint(canvas);
    }
  }
  if (inactive_tab_alpha < 255)
    canvas->Restore();

  if (GetWidget()->ShouldUseNativeFrame()) {
    // Make sure non-active tabs are somewhat transparent.
    SkPaint paint;
    // If there are multiple tabs selected, fade non-selected tabs more to make
    // the selected tabs more noticable.
    int alpha = selected_tab_count > 1 ?
        kNativeFrameInactiveTabAlphaMultiSelection :
        kNativeFrameInactiveTabAlpha;
    paint.setColor(SkColorSetARGB(alpha, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    // The tabstrip area overlaps the toolbar area by 2 px.
    canvas->DrawRect(gfx::Rect(0, 0, width(), height() - 2), paint);
  }

  // Now selected but not active. We don't want these dimmed if using native
  // frame, so they're painted after initial pass.
  for (size_t i = 0; i < selected_tabs.size(); ++i)
    selected_tabs[i]->Paint(canvas);

  // Next comes the active tab.
  if (active_tab && !is_dragging)
    active_tab->Paint(canvas);

  // Paint the New Tab button.
  if (inactive_tab_alpha < 255)
    canvas->SaveLayerAlpha(inactive_tab_alpha);
  newtab_button_->Paint(canvas);
  if (inactive_tab_alpha < 255)
    canvas->Restore();

  // And the dragged tabs.
  for (size_t i = 0; i < tabs_dragging.size(); ++i)
    tabs_dragging[i]->Paint(canvas);

  // If the active tab is being dragged, it goes last.
  if (active_tab && is_dragging)
    active_tab->Paint(canvas);
}

std::string TabStrip::GetClassName() const {
  return kViewClassName;
}

gfx::Size TabStrip::GetPreferredSize() {
  // For stacked tabs the minimum size is calculated as the size needed to
  // handle showing any number of tabs. Otherwise report the minimum width as
  // the size required for a single selected tab plus the new tab button. Don't
  // base it on the actual number of tabs because it's undesirable to have the
  // minimum window size change when a new tab is opened.
  int needed_width;
  if (touch_layout_.get() || adjust_layout_) {
    needed_width = Tab::GetTouchWidth() +
        (2 * kStackedPadding * kMaxStackedCount);
  } else {
    needed_width = Tab::GetMinimumSelectedSize().width();
  }
  needed_width += new_tab_button_width();
  if (immersive_style_)
    return gfx::Size(needed_width, Tab::GetImmersiveHeight());
  return gfx::Size(needed_width, Tab::GetMinimumUnselectedSize().height());
}

void TabStrip::OnDragEntered(const DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  UpdateDropIndex(event);
}

int TabStrip::OnDragUpdated(const DropTargetEvent& event) {
  UpdateDropIndex(event);
  return GetDropEffect(event);
}

void TabStrip::OnDragExited() {
  SetDropIndex(-1, false);
}

int TabStrip::OnPerformDrop(const DropTargetEvent& event) {
  if (!drop_info_.get())
    return ui::DragDropTypes::DRAG_NONE;

  const int drop_index = drop_info_->drop_index;
  const bool drop_before = drop_info_->drop_before;

  // Hide the drop indicator.
  SetDropIndex(-1, false);

  GURL url;
  string16 title;
  if (!event.data().GetURLAndTitle(&url, &title) || !url.is_valid())
    return ui::DragDropTypes::DRAG_NONE;

  controller()->PerformDrop(drop_before, drop_index, url);

  return GetDropEffect(event);
}

void TabStrip::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PAGETABLIST;
}

views::View* TabStrip::GetEventHandlerForPoint(const gfx::Point& point) {
  if (!touch_layout_.get()) {
    // Return any view that isn't a Tab or this TabStrip immediately. We don't
    // want to interfere.
    views::View* v = View::GetEventHandlerForPoint(point);
    if (v && v != this && v->GetClassName() != Tab::kViewClassName)
      return v;

    // The display order doesn't necessarily match the child list order, so we
    // walk the display list hit-testing Tabs. Since the active tab always
    // renders on top of adjacent tabs, it needs to be hit-tested before any
    // left-adjacent Tab, so we look ahead for it as we walk.
    for (int i = 0; i < tab_count(); ++i) {
      Tab* next_tab = i < (tab_count() - 1) ? tab_at(i + 1) : NULL;
      if (next_tab && next_tab->IsActive() && IsPointInTab(next_tab, point))
        return next_tab;
      if (IsPointInTab(tab_at(i), point))
        return tab_at(i);
    }
  } else {
    if (newtab_button_->visible()) {
      views::View* view =
          ConvertPointToViewAndGetEventHandler(this, newtab_button_, point);
      if (view)
        return view;
    }
    Tab* tab = FindTabForEvent(point);
    if (tab)
      return ConvertPointToViewAndGetEventHandler(this, tab, point);
  }
  return this;
}

// static
int TabStrip::GetImmersiveHeight() {
  return Tab::GetImmersiveHeight();
}

int TabStrip::GetMiniTabCount() const {
  int mini_count = 0;
  while (mini_count < tab_count() && tab_at(mini_count)->data().mini)
    mini_count++;
  return mini_count;
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::ButtonListener implementation:

void TabStrip::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == newtab_button_) {
    content::RecordAction(UserMetricsAction("NewTab_Button"));
    UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_BUTTON,
                              TabStripModel::NEW_TAB_ENUM_COUNT);
    controller()->CreateNewTab();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, protected:

// Overridden to support automation. See automation_proxy_uitest.cc.
const views::View* TabStrip::GetViewByID(int view_id) const {
  if (tab_count() > 0) {
    if (view_id == VIEW_ID_TAB_LAST) {
      return tab_at(tab_count() - 1);
    } else if ((view_id >= VIEW_ID_TAB_0) && (view_id < VIEW_ID_TAB_LAST)) {
      int index = view_id - VIEW_ID_TAB_0;
      if (index >= 0 && index < tab_count()) {
        return tab_at(index);
      } else {
        return NULL;
      }
    }
  }

  return View::GetViewByID(view_id);
}

bool TabStrip::OnMousePressed(const ui::MouseEvent& event) {
  UpdateLayoutTypeFromMouseEvent(this, event);
  // We can't return true here, else clicking in an empty area won't drag the
  // window.
  return false;
}

bool TabStrip::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueDrag(this, event);
  return true;
}

void TabStrip::OnMouseReleased(const ui::MouseEvent& event) {
  EndDrag(END_DRAG_COMPLETE);
  UpdateLayoutTypeFromMouseEvent(this, event);
}

void TabStrip::OnMouseCaptureLost() {
  EndDrag(END_DRAG_CAPTURE_LOST);
}

void TabStrip::OnMouseMoved(const ui::MouseEvent& event) {
  UpdateLayoutTypeFromMouseEvent(this, event);
}

void TabStrip::OnMouseEntered(const ui::MouseEvent& event) {
  SetResetToShrinkOnExit(true);
}

void TabStrip::OnGestureEvent(ui::GestureEvent* event) {
  SetResetToShrinkOnExit(false);
  switch (event->type()) {
    case ui::ET_GESTURE_END:
      EndDrag(END_DRAG_COMPLETE);
      if (adjust_layout_) {
        SetLayoutType(TAB_STRIP_LAYOUT_STACKED, true);
        controller_->LayoutTypeMaybeChanged();
      }
      break;

    case ui::ET_GESTURE_LONG_PRESS:
      if (drag_controller_.get())
        drag_controller_->SetMoveBehavior(TabDragController::REORDER);
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      ContinueDrag(this, *event);
      break;

    case ui::ET_GESTURE_BEGIN:
      EndDrag(END_DRAG_CANCEL);
      break;

    default:
      break;
  }
  event->SetHandled();
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, private:

void TabStrip::Init() {
  set_id(VIEW_ID_TAB_STRIP);
  // So we get enter/exit on children to switch layout type.
  set_notify_enter_exit_on_child(true);
  newtab_button_bounds_.SetRect(0,
                                0,
                                newtab_button_asset_width(),
                                newtab_button_asset_height() +
                                    newtab_button_v_offset());
  newtab_button_ = new NewTabButton(this, this);
  newtab_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  newtab_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));
  newtab_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_BOTTOM);
  AddChildView(newtab_button_);
  if (drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image = GetDropArrowImage(true);
    drop_indicator_width = drop_image->width();
    drop_indicator_height = drop_image->height();
  }
}

Tab* TabStrip::CreateTab() {
  Tab* tab = new Tab(this);
  tab->set_animation_container(animation_container_.get());
  return tab;
}

void TabStrip::StartInsertTabAnimation(int model_index) {
  PrepareForAnimation();

  // The TabStrip can now use its entire width to lay out Tabs.
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  GenerateIdealBounds();

  Tab* tab = tab_at(model_index);
  if (model_index == 0) {
    tab->SetBounds(0, ideal_bounds(model_index).y(), 0,
                   ideal_bounds(model_index).height());
  } else {
    Tab* last_tab = tab_at(model_index - 1);
    tab->SetBounds(last_tab->bounds().right() + tab_h_offset(),
                   ideal_bounds(model_index).y(), 0,
                   ideal_bounds(model_index).height());
  }

  AnimateToIdealBounds();
}

void TabStrip::StartMoveTabAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartRemoveTabAnimation(int model_index) {
  PrepareForAnimation();

  // Mark the tab as closing.
  Tab* tab = tab_at(model_index);
  tab->set_closing(true);

  RemoveTabFromViewModel(model_index);

  ScheduleRemoveTabAnimation(tab);
}

void TabStrip::ScheduleRemoveTabAnimation(Tab* tab) {
  // Start an animation for the tabs.
  GenerateIdealBounds();
  AnimateToIdealBounds();

  // Animate the tab being closed to 0x0.
  gfx::Rect tab_bounds = tab->bounds();
  tab_bounds.set_width(0);
  bounds_animator_.AnimateViewTo(tab, tab_bounds);

  // Register delegate to do cleanup when done, BoundsAnimator takes
  // ownership of RemoveTabDelegate.
  bounds_animator_.SetAnimationDelegate(tab, new RemoveTabDelegate(this, tab),
                                        true);

  // Don't animate the new tab button when dragging tabs. Otherwise it looks
  // like the new tab button magically appears from beyond the end of the tab
  // strip.
  if (TabDragController::IsAttachedTo(this)) {
    bounds_animator_.StopAnimatingView(newtab_button_);
    newtab_button_->SetBoundsRect(newtab_button_bounds_);
  }
}

void TabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    if (!tab->dragging())
      bounds_animator_.AnimateViewTo(tab, ideal_bounds(i));
  }

  bounds_animator_.AnimateViewTo(newtab_button_, newtab_button_bounds_);
}

bool TabStrip::ShouldHighlightCloseButtonAfterRemove() {
  return in_tab_close_;
}

void TabStrip::DoLayout() {
  last_layout_size_ = size();

  StopAnimating(false);

  SwapLayoutIfNecessary();

  if (touch_layout_.get())
    touch_layout_->SetWidth(size().width() - new_tab_button_width());

  GenerateIdealBounds();

  views::ViewModelUtils::SetViewBoundsToIdealBounds(tabs_);

  SchedulePaint();

  bounds_animator_.StopAnimatingView(newtab_button_);
  newtab_button_->SetBoundsRect(newtab_button_bounds_);
}

void TabStrip::DragActiveTab(const std::vector<int>& initial_positions,
                             int delta) {
  DCHECK_EQ(tab_count(), static_cast<int>(initial_positions.size()));
  if (!touch_layout_.get()) {
    StackDraggedTabs(delta);
    return;
  }
  SetIdealBoundsFromPositions(initial_positions);
  touch_layout_->DragActiveTab(delta);
  DoLayout();
}

void TabStrip::SetIdealBoundsFromPositions(const std::vector<int>& positions) {
  if (static_cast<size_t>(tab_count()) != positions.size())
    return;

  for (int i = 0; i < tab_count(); ++i) {
    gfx::Rect bounds(ideal_bounds(i));
    bounds.set_x(positions[i]);
    set_ideal_bounds(i, bounds);
  }
}

void TabStrip::StackDraggedTabs(int delta) {
  DCHECK(!touch_layout_.get());
  GenerateIdealBounds();
  const int active_index = controller_->GetActiveIndex();
  DCHECK_NE(-1, active_index);
  if (delta < 0) {
    // Drag the tabs to the left, stacking tabs before the active tab.
    const int adjusted_delta =
        std::min(ideal_bounds(active_index).x() -
                     kStackedPadding * std::min(active_index, kMaxStackedCount),
                 -delta);
    for (int i = 0; i <= active_index; ++i) {
      const int min_x = std::min(i, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      new_bounds.set_x(std::max(min_x, new_bounds.x() - adjusted_delta));
      set_ideal_bounds(i, new_bounds);
    }
    const bool is_active_mini = tab_at(active_index)->data().mini;
    const int active_width = ideal_bounds(active_index).width();
    for (int i = active_index + 1; i < tab_count(); ++i) {
      const int max_x = ideal_bounds(active_index).x() +
          (kStackedPadding * std::min(i - active_index, kMaxStackedCount));
      gfx::Rect new_bounds(ideal_bounds(i));
      int new_x = std::max(new_bounds.x() + delta, max_x);
      if (new_x == max_x && !tab_at(i)->data().mini && !is_active_mini &&
          new_bounds.width() != active_width)
        new_x += (active_width - new_bounds.width());
      new_bounds.set_x(new_x);
      set_ideal_bounds(i, new_bounds);
    }
  } else {
    // Drag the tabs to the right, stacking tabs after the active tab.
    const int last_tab_width = ideal_bounds(tab_count() - 1).width();
    const int last_tab_x = width() - new_tab_button_width() - last_tab_width;
    if (active_index == tab_count() - 1 &&
        ideal_bounds(tab_count() - 1).x() == last_tab_x)
      return;
    const int adjusted_delta =
        std::min(last_tab_x -
                     kStackedPadding * std::min(tab_count() - active_index - 1,
                                                kMaxStackedCount) -
                     ideal_bounds(active_index).x(),
                 delta);
    for (int last_index = tab_count() - 1, i = last_index; i >= active_index;
         --i) {
      const int max_x = last_tab_x -
          std::min(tab_count() - i - 1, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      int new_x = std::min(max_x, new_bounds.x() + adjusted_delta);
      // Because of rounding not all tabs are the same width. Adjust the
      // position to accommodate this, otherwise the stacking is off.
      if (new_x == max_x && !tab_at(i)->data().mini &&
          new_bounds.width() != last_tab_width)
        new_x += (last_tab_width - new_bounds.width());
      new_bounds.set_x(new_x);
      set_ideal_bounds(i, new_bounds);
    }
    for (int i = active_index - 1; i >= 0; --i) {
      const int min_x = ideal_bounds(active_index).x() -
          std::min(active_index - i, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      new_bounds.set_x(std::min(min_x, new_bounds.x() + delta));
      set_ideal_bounds(i, new_bounds);
    }
    if (ideal_bounds(tab_count() - 1).right() >= newtab_button_->x())
      newtab_button_->SetVisible(false);
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(tabs_);
  SchedulePaint();
}

bool TabStrip::IsStackingDraggedTabs() const {
  return drag_controller_.get() && drag_controller_->started_drag() &&
      (drag_controller_->move_behavior() ==
       TabDragController::MOVE_VISIBILE_TABS);
}

void TabStrip::LayoutDraggedTabsAt(const std::vector<Tab*>& tabs,
                                   Tab* active_tab,
                                   const gfx::Point& location,
                                   bool initial_drag) {
  // Immediately hide the new tab button if the last tab is being dragged.
  if (tab_at(tab_count() - 1)->dragging())
    newtab_button_->SetVisible(false);
  std::vector<gfx::Rect> bounds;
  CalculateBoundsForDraggedTabs(tabs, &bounds);
  DCHECK_EQ(tabs.size(), bounds.size());
  int active_tab_model_index = GetModelIndexOfTab(active_tab);
  int active_tab_index = static_cast<int>(
      std::find(tabs.begin(), tabs.end(), active_tab) - tabs.begin());
  for (size_t i = 0; i < tabs.size(); ++i) {
    Tab* tab = tabs[i];
    gfx::Rect new_bounds = bounds[i];
    new_bounds.Offset(location.x(), location.y());
    int consecutive_index =
        active_tab_model_index - (active_tab_index - static_cast<int>(i));
    // If this is the initial layout during a drag and the tabs aren't
    // consecutive animate the view into position. Do the same if the tab is
    // already animating (which means we previously caused it to animate).
    if ((initial_drag &&
         GetModelIndexOfTab(tabs[i]) != consecutive_index) ||
        bounds_animator_.IsAnimating(tabs[i])) {
      bounds_animator_.SetTargetBounds(tabs[i], new_bounds);
    } else {
      tab->SetBoundsRect(new_bounds);
    }
  }
}

void TabStrip::CalculateBoundsForDraggedTabs(const std::vector<Tab*>& tabs,
                                             std::vector<gfx::Rect>* bounds) {
  int x = 0;
  for (size_t i = 0; i < tabs.size(); ++i) {
    Tab* tab = tabs[i];
    if (i > 0 && tab->data().mini != tabs[i - 1]->data().mini)
      x += kMiniToNonMiniGap;
    gfx::Rect new_bounds = tab->bounds();
    new_bounds.set_origin(gfx::Point(x, 0));
    bounds->push_back(new_bounds);
    x += tab->width() + tab_h_offset();
  }
}

int TabStrip::GetSizeNeededForTabs(const std::vector<Tab*>& tabs) {
  int width = 0;
  for (size_t i = 0; i < tabs.size(); ++i) {
    Tab* tab = tabs[i];
    width += tab->width();
    if (i > 0 && tab->data().mini != tabs[i - 1]->data().mini)
      width += kMiniToNonMiniGap;
  }
  if (tabs.size() > 0)
    width += tab_h_offset() * static_cast<int>(tabs.size() - 1);
  return width;
}

void TabStrip::RemoveTabFromViewModel(int index) {
  // We still need to paint the tab until we actually remove it. Put it
  // in tabs_closing_map_ so we can find it.
  tabs_closing_map_[index].push_back(tab_at(index));
  UpdateTabsClosingMap(index + 1, -1);
  tabs_.Remove(index);
}

void TabStrip::RemoveAndDeleteTab(Tab* tab) {
  scoped_ptr<Tab> deleter(tab);
  for (TabsClosingMap::iterator i(tabs_closing_map_.begin());
       i != tabs_closing_map_.end(); ++i) {
    std::vector<Tab*>::iterator j =
        std::find(i->second.begin(), i->second.end(), tab);
    if (j != i->second.end()) {
      i->second.erase(j);
      if (i->second.empty())
        tabs_closing_map_.erase(i);
      return;
    }
  }
  NOTREACHED();
}

void TabStrip::UpdateTabsClosingMap(int index, int delta) {
  if (tabs_closing_map_.empty())
    return;

  if (delta == -1 &&
      tabs_closing_map_.find(index - 1) != tabs_closing_map_.end() &&
      tabs_closing_map_.find(index) != tabs_closing_map_.end()) {
    const std::vector<Tab*>& tabs(tabs_closing_map_[index]);
    tabs_closing_map_[index - 1].insert(
        tabs_closing_map_[index - 1].end(), tabs.begin(), tabs.end());
  }
  TabsClosingMap updated_map;
  for (TabsClosingMap::iterator i(tabs_closing_map_.begin());
       i != tabs_closing_map_.end(); ++i) {
    if (i->first > index)
      updated_map[i->first + delta] = i->second;
    else if (i->first < index)
      updated_map[i->first] = i->second;
  }
  if (delta > 0 && tabs_closing_map_.find(index) != tabs_closing_map_.end())
    updated_map[index + delta] = tabs_closing_map_[index];
  tabs_closing_map_.swap(updated_map);
}

void TabStrip::StartedDraggingTabs(const std::vector<Tab*>& tabs) {
  // Hide the new tab button immediately if we didn't originate the drag.
  if (!drag_controller_.get())
    newtab_button_->SetVisible(false);

  PrepareForAnimation();

  // Reset dragging state of existing tabs.
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->set_dragging(false);

  for (size_t i = 0; i < tabs.size(); ++i) {
    tabs[i]->set_dragging(true);
    bounds_animator_.StopAnimatingView(tabs[i]);
  }

  // Move the dragged tabs to their ideal bounds.
  GenerateIdealBounds();

  // Sets the bounds of the dragged tabs.
  for (size_t i = 0; i < tabs.size(); ++i) {
    int tab_data_index = GetModelIndexOfTab(tabs[i]);
    DCHECK_NE(-1, tab_data_index);
    tabs[i]->SetBoundsRect(ideal_bounds(tab_data_index));
  }
  SchedulePaint();
}

void TabStrip::DraggedTabsDetached() {
  newtab_button_->SetVisible(true);
}

void TabStrip::StoppedDraggingTabs(const std::vector<Tab*>& tabs,
                                   const std::vector<int>& initial_positions,
                                   bool move_only,
                                   bool completed) {
  newtab_button_->SetVisible(true);
  if (move_only && touch_layout_.get()) {
    if (completed) {
      touch_layout_->SizeToFit();
    } else {
      SetIdealBoundsFromPositions(initial_positions);
    }
  }
  bool is_first_tab = true;
  for (size_t i = 0; i < tabs.size(); ++i)
    StoppedDraggingTab(tabs[i], &is_first_tab);
}

void TabStrip::StoppedDraggingTab(Tab* tab, bool* is_first_tab) {
  int tab_data_index = GetModelIndexOfTab(tab);
  if (tab_data_index == -1) {
    // The tab was removed before the drag completed. Don't do anything.
    return;
  }

  if (*is_first_tab) {
    *is_first_tab = false;
    PrepareForAnimation();

    // Animate the view back to its correct position.
    GenerateIdealBounds();
    AnimateToIdealBounds();
  }
  bounds_animator_.AnimateViewTo(tab, ideal_bounds(tab_data_index));
  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator_.SetAnimationDelegate(
      tab, new ResetDraggingStateDelegate(tab), true);
}

void TabStrip::OwnDragController(TabDragController* controller) {
  // Typically, ReleaseDragController() and OwnDragController() calls are paired
  // via corresponding calls to TabDragController::Detach() and
  // TabDragController::Attach(). There is one exception to that rule: when a
  // drag might start, we create a TabDragController that is owned by the
  // potential source tabstrip in MaybeStartDrag(). If a drag actually starts,
  // we then call Attach() on the source tabstrip, but since the source tabstrip
  // already owns the TabDragController, so we don't need to do anything.
  if (controller != drag_controller_.get())
    drag_controller_.reset(controller);
}

void TabStrip::DestroyDragController() {
  newtab_button_->SetVisible(true);
  drag_controller_.reset();
}

TabDragController* TabStrip::ReleaseDragController() {
  return drag_controller_.release();
}

void TabStrip::PaintClosingTabs(gfx::Canvas* canvas, int index) {
  if (tabs_closing_map_.find(index) == tabs_closing_map_.end())
    return;

  const std::vector<Tab*>& tabs = tabs_closing_map_[index];
  for (std::vector<Tab*>::const_reverse_iterator i(tabs.rbegin());
       i != tabs.rend(); ++i) {
    (*i)->Paint(canvas);
  }
}

void TabStrip::UpdateLayoutTypeFromMouseEvent(views::View* source,
                                              const ui::MouseEvent& event) {
  if (!GetAdjustLayout())
    return;

  // The following code attempts to switch to TAB_STRIP_LAYOUT_SHRINK when the
  // mouse exits the tabstrip (or the mouse is pressed on a stacked tab) and
  // TAB_STRIP_LAYOUT_STACKED when a touch device is used. This is made
  // problematic by windows generating mouse move events that do not clearly
  // indicate the move is the result of a touch device. This assumes a real
  // mouse is used if |kMouseMoveCountBeforeConsiderReal| mouse move events are
  // received within the time window |kMouseMoveTimeMS|.  At the time we get a
  // mouse press we know whether its from a touch device or not, but we don't
  // layout then else everything shifts. Instead we wait for the release.
  //
  // TODO(sky): revisit this when touch events are really plumbed through.

  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      mouse_move_count_ = 0;
      last_mouse_move_time_ = base::TimeTicks();
      SetResetToShrinkOnExit((event.flags() & ui::EF_FROM_TOUCH) == 0);
      if (reset_to_shrink_on_exit_ && touch_layout_.get()) {
        gfx::Point tab_strip_point(event.location());
        views::View::ConvertPointToTarget(source, this, &tab_strip_point);
        Tab* tab = FindTabForEvent(tab_strip_point);
        if (tab && touch_layout_->IsStacked(GetModelIndexOfTab(tab))) {
          SetLayoutType(TAB_STRIP_LAYOUT_SHRINK, true);
          controller_->LayoutTypeMaybeChanged();
        }
      }
      break;

    case ui::ET_MOUSE_MOVED: {
#if defined(USE_ASH)
      // Ash does not synthesize mouse events from touch events.
      SetResetToShrinkOnExit(true);
#else
      gfx::Point location(event.location());
      ConvertPointToTarget(source, this, &location);
      if (location == last_mouse_move_location_)
        return;  // Ignore spurious moves.
      last_mouse_move_location_ = location;
      if ((event.flags() & ui::EF_FROM_TOUCH) == 0 &&
          (event.flags() & ui::EF_IS_SYNTHESIZED) == 0) {
        if ((base::TimeTicks::Now() - last_mouse_move_time_).InMilliseconds() <
            kMouseMoveTimeMS) {
          if (mouse_move_count_++ == kMouseMoveCountBeforeConsiderReal)
            SetResetToShrinkOnExit(true);
        } else {
          mouse_move_count_ = 1;
          last_mouse_move_time_ = base::TimeTicks::Now();
        }
      } else {
        last_mouse_move_time_ = base::TimeTicks();
      }
#endif
      break;
    }

    case ui::ET_MOUSE_RELEASED: {
      gfx::Point location(event.location());
      ConvertPointToTarget(source, this, &location);
      last_mouse_move_location_ = location;
      mouse_move_count_ = 0;
      last_mouse_move_time_ = base::TimeTicks();
      if ((event.flags() & ui::EF_FROM_TOUCH) == ui::EF_FROM_TOUCH) {
        SetLayoutType(TAB_STRIP_LAYOUT_STACKED, true);
        controller_->LayoutTypeMaybeChanged();
      }
      break;
    }

    default:
      break;
  }
}

void TabStrip::GetCurrentTabWidths(double* unselected_width,
                                   double* selected_width) const {
  *unselected_width = current_unselected_width_;
  *selected_width = current_selected_width_;
}

void TabStrip::GetDesiredTabWidths(int tab_count,
                                   int mini_tab_count,
                                   double* unselected_width,
                                   double* selected_width) const {
  DCHECK(tab_count >= 0 && mini_tab_count >= 0 && mini_tab_count <= tab_count);
  const double min_unselected_width = Tab::GetMinimumUnselectedSize().width();
  const double min_selected_width = Tab::GetMinimumSelectedSize().width();

  *unselected_width = min_unselected_width;
  *selected_width = min_selected_width;

  if (tab_count == 0) {
    // Return immediately to avoid divide-by-zero below.
    return;
  }

  // Determine how much space we can actually allocate to tabs.
  int available_width;
  if (available_width_for_tabs_ < 0) {
    available_width = width() - new_tab_button_width();
  } else {
    // Interesting corner case: if |available_width_for_tabs_| > the result
    // of the calculation in the conditional arm above, the strip is in
    // overflow.  We can either use the specified width or the true available
    // width here; the first preserves the consistent "leave the last tab under
    // the user's mouse so they can close many tabs" behavior at the cost of
    // prolonging the glitchy appearance of the overflow state, while the second
    // gets us out of overflow as soon as possible but forces the user to move
    // their mouse for a few tabs' worth of closing.  We choose visual
    // imperfection over behavioral imperfection and select the first option.
    available_width = available_width_for_tabs_;
  }

  if (mini_tab_count > 0) {
    available_width -= mini_tab_count * (Tab::GetMiniWidth() + tab_h_offset());
    tab_count -= mini_tab_count;
    if (tab_count == 0) {
      *selected_width = *unselected_width = Tab::GetStandardSize().width();
      return;
    }
    // Account for gap between the last mini-tab and first non-mini-tab.
    available_width -= kMiniToNonMiniGap;
  }

  // Calculate the desired tab widths by dividing the available space into equal
  // portions.  Don't let tabs get larger than the "standard width" or smaller
  // than the minimum width for each type, respectively.
  const int total_offset = tab_h_offset() * (tab_count - 1);
  const double desired_tab_width = std::min((static_cast<double>(
      available_width - total_offset) / static_cast<double>(tab_count)),
      static_cast<double>(Tab::GetStandardSize().width()));
  *unselected_width = std::max(desired_tab_width, min_unselected_width);
  *selected_width = std::max(desired_tab_width, min_selected_width);

  // When there are multiple tabs, we'll have one selected and some unselected
  // tabs.  If the desired width was between the minimum sizes of these types,
  // try to shrink the tabs with the smaller minimum.  For example, if we have
  // a strip of width 10 with 4 tabs, the desired width per tab will be 2.5.  If
  // selected tabs have a minimum width of 4 and unselected tabs have a minimum
  // width of 1, the above code would set *unselected_width = 2.5,
  // *selected_width = 4, which results in a total width of 11.5.  Instead, we
  // want to set *unselected_width = 2, *selected_width = 4, for a total width
  // of 10.
  if (tab_count > 1) {
    if ((min_unselected_width < min_selected_width) &&
        (desired_tab_width < min_selected_width)) {
      // Unselected width = (total width - selected width) / (num_tabs - 1)
      *unselected_width = std::max(static_cast<double>(
          available_width - total_offset - min_selected_width) /
          static_cast<double>(tab_count - 1), min_unselected_width);
    } else if ((min_unselected_width > min_selected_width) &&
               (desired_tab_width < min_unselected_width)) {
      // Selected width = (total width - (unselected width * (num_tabs - 1)))
      *selected_width = std::max(available_width - total_offset -
          (min_unselected_width * (tab_count - 1)), min_selected_width);
    }
  }
}

void TabStrip::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (tab_count() == 0)
    return;

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  in_tab_close_ = false;
  available_width_for_tabs_ = -1;
  int mini_tab_count = GetMiniTabCount();
  if (mini_tab_count == tab_count()) {
    // Only mini-tabs, we know the tab widths won't have changed (all
    // mini-tabs have the same width), so there is nothing to do.
    return;
  }
  // Don't try and avoid layout based on tab sizes. If tabs are small enough
  // then the width of the active tab may not change, but other widths may
  // have. This is particularly important if we've overflowed (all tabs are at
  // the min).
  StartResizeLayoutAnimation();
}

void TabStrip::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!drag_controller_.get())
    ResizeLayoutTabs();
  else
    StartResizeLayoutTabsFromTouchTimer();
}

void TabStrip::StartResizeLayoutTabsFromTouchTimer() {
  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kTouchResizeLayoutTimeMS),
      this, &TabStrip::ResizeLayoutTabsFromTouch);
}

void TabStrip::SetTabBoundsForDrag(const std::vector<gfx::Rect>& tab_bounds) {
  StopAnimating(false);
  DCHECK_EQ(tab_count(), static_cast<int>(tab_bounds.size()));
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->SetBoundsRect(tab_bounds[i]);
}

void TabStrip::AddMessageLoopObserver() {
  if (!mouse_watcher_.get()) {
    mouse_watcher_.reset(
        new views::MouseWatcher(
            new views::MouseWatcherViewHost(
                this, gfx::Insets(0, 0, kTabStripAnimationVSlop, 0)),
            this));
  }
  mouse_watcher_->Start();
}

void TabStrip::RemoveMessageLoopObserver() {
  mouse_watcher_.reset(NULL);
}

gfx::Rect TabStrip::GetDropBounds(int drop_index,
                                  bool drop_before,
                                  bool* is_beneath) {
  DCHECK_NE(drop_index, -1);
  int center_x;
  if (drop_index < tab_count()) {
    Tab* tab = tab_at(drop_index);
    if (drop_before)
      center_x = tab->x() - (tab_h_offset() / 2);
    else
      center_x = tab->x() + (tab->width() / 2);
  } else {
    Tab* last_tab = tab_at(drop_index - 1);
    center_x = last_tab->x() + last_tab->width() + (tab_h_offset() / 2);
  }

  // Mirror the center point if necessary.
  center_x = GetMirroredXInView(center_x);

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - drop_indicator_width / 2,
                      -drop_indicator_height);
  ConvertPointToScreen(this, &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), drop_indicator_width,
                        drop_indicator_height);

  // If the rect doesn't fit on the monitor, push the arrow to the bottom.
#if defined(OS_WIN) && !defined(USE_AURA)
  gfx::Rect monitor_bounds = views::GetMonitorBoundsForRect(drop_bounds);
  *is_beneath = (monitor_bounds.IsEmpty() ||
                 !monitor_bounds.Contains(drop_bounds));
#else
  *is_beneath = false;
  NOTIMPLEMENTED();
#endif
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());

  return drop_bounds;
}

void TabStrip::UpdateDropIndex(const DropTargetEvent& event) {
  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());
  // We don't allow replacing the urls of mini-tabs.
  for (int i = GetMiniTabCount(); i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    const int tab_max_x = tab->x() + tab->width();
    const int hot_width = tab->width() / kTabEdgeRatioInverse;
    if (x < tab_max_x) {
      if (x < tab->x() + hot_width)
        SetDropIndex(i, true);
      else if (x >= tab_max_x - hot_width)
        SetDropIndex(i + 1, true);
      else
        SetDropIndex(i, false);
      return;
    }
  }

  // The drop isn't over a tab, add it to the end.
  SetDropIndex(tab_count(), true);
}

void TabStrip::SetDropIndex(int tab_data_index, bool drop_before) {
  // Let the controller know of the index update.
  controller()->OnDropIndexUpdate(tab_data_index, drop_before);

  if (tab_data_index == -1) {
    if (drop_info_.get())
      drop_info_.reset(NULL);
    return;
  }

  if (drop_info_.get() && drop_info_->drop_index == tab_data_index &&
      drop_info_->drop_before == drop_before) {
    return;
  }

  bool is_beneath;
  gfx::Rect drop_bounds = GetDropBounds(tab_data_index, drop_before,
                                        &is_beneath);

  if (!drop_info_.get()) {
    drop_info_.reset(
        new DropInfo(tab_data_index, drop_before, !is_beneath, GetWidget()));
  } else {
    drop_info_->drop_index = tab_data_index;
    drop_info_->drop_before = drop_before;
    if (is_beneath == drop_info_->point_down) {
      drop_info_->point_down = !is_beneath;
      drop_info_->arrow_view->SetImage(
          GetDropArrowImage(drop_info_->point_down));
    }
  }

  // Reposition the window. Need to show it too as the window is initially
  // hidden.
  drop_info_->arrow_window->SetBounds(drop_bounds);
  drop_info_->arrow_window->Show();
}

int TabStrip::GetDropEffect(const ui::DropTargetEvent& event) {
  const int source_ops = event.source_operations();
  if (source_ops & ui::DragDropTypes::DRAG_COPY)
    return ui::DragDropTypes::DRAG_COPY;
  if (source_ops & ui::DragDropTypes::DRAG_LINK)
    return ui::DragDropTypes::DRAG_LINK;
  return ui::DragDropTypes::DRAG_MOVE;
}

// static
gfx::ImageSkia* TabStrip::GetDropArrowImage(bool is_down) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStrip::DropInfo ----------------------------------------------------------

TabStrip::DropInfo::DropInfo(int drop_index,
                             bool drop_before,
                             bool point_down,
                             views::Widget* context)
    : drop_index(drop_index),
      drop_before(drop_before),
      point_down(point_down) {
  arrow_view = new views::ImageView;
  arrow_view->SetImage(GetDropArrowImage(point_down));

  arrow_window = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.keep_on_top = true;
  params.transparent = true;
  params.accept_events = false;
  params.can_activate = false;
  params.bounds = gfx::Rect(drop_indicator_width, drop_indicator_height);
  params.context = context->GetNativeView();
  arrow_window->Init(params);
  arrow_window->SetContentsView(arrow_view);
}

TabStrip::DropInfo::~DropInfo() {
  // Close eventually deletes the window, which deletes arrow_view too.
  arrow_window->Close();
}

///////////////////////////////////////////////////////////////////////////////

void TabStrip::PrepareForAnimation() {
  if (!IsDragSessionActive() && !TabDragController::IsAttachedTo(this)) {
    for (int i = 0; i < tab_count(); ++i)
      tab_at(i)->set_dragging(false);
  }
}

void TabStrip::GenerateIdealBounds() {
  int new_tab_y = 0;

  if (touch_layout_.get()) {
    if (tabs_.view_size() == 0)
      return;

    int new_tab_x = tabs_.ideal_bounds(tabs_.view_size() - 1).right() +
        newtab_button_h_offset();
    newtab_button_bounds_.set_origin(gfx::Point(new_tab_x, new_tab_y));
    return;
  }

  double unselected, selected;
  GetDesiredTabWidths(tab_count(), GetMiniTabCount(), &unselected, &selected);
  current_unselected_width_ = unselected;
  current_selected_width_ = selected;

  // NOTE: This currently assumes a tab's height doesn't differ based on
  // selected state or the number of tabs in the strip!
  int tab_height = Tab::GetStandardSize().height();
  int first_non_mini_index = 0;
  double tab_x = GenerateIdealBoundsForMiniTabs(&first_non_mini_index);
  for (int i = first_non_mini_index; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    DCHECK(!tab->data().mini);
    double tab_width = tab->IsActive() ? selected : unselected;
    double end_of_tab = tab_x + tab_width;
    int rounded_tab_x = Round(tab_x);
    set_ideal_bounds(
        i,
        gfx::Rect(rounded_tab_x, 0, Round(end_of_tab) - rounded_tab_x,
                  tab_height));
    tab_x = end_of_tab + tab_h_offset();
  }

  // Update bounds of new tab button.
  int new_tab_x;
  if (abs(Round(unselected) - Tab::GetStandardSize().width()) > 1 &&
      !in_tab_close_) {
    // We're shrinking tabs, so we need to anchor the New Tab button to the
    // right edge of the TabStrip's bounds, rather than the right edge of the
    // right-most Tab, otherwise it'll bounce when animating.
    new_tab_x = width() - newtab_button_bounds_.width();
  } else {
    new_tab_x = Round(tab_x - tab_h_offset()) + newtab_button_h_offset();
  }
  newtab_button_bounds_.set_origin(gfx::Point(new_tab_x, new_tab_y));
}

int TabStrip::GenerateIdealBoundsForMiniTabs(int* first_non_mini_index) {
  int next_x = 0;
  int mini_width = Tab::GetMiniWidth();
  int tab_height = Tab::GetStandardSize().height();
  int index = 0;
  for (; index < tab_count() && tab_at(index)->data().mini; ++index) {
    set_ideal_bounds(index,
                     gfx::Rect(next_x, 0, mini_width, tab_height));
    next_x += mini_width + tab_h_offset();
  }
  if (index > 0 && index < tab_count())
    next_x += kMiniToNonMiniGap;
  if (first_non_mini_index)
    *first_non_mini_index = index;
  return next_x;
}

int TabStrip::new_tab_button_width() const {
  return newtab_button_asset_width() + newtab_button_h_offset();
}

void TabStrip::StartResizeLayoutAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartMiniTabAnimation() {
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  PrepareForAnimation();

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartMouseInitiatedRemoveTabAnimation(int model_index) {
  // The user initiated the close. We want to persist the bounds of all the
  // existing tabs, so we manually shift ideal_bounds then animate.
  Tab* tab_closing = tab_at(model_index);
  int delta = tab_closing->width() + tab_h_offset();
  // If the tab being closed is a mini-tab next to a non-mini-tab, be sure to
  // add the extra padding.
  DCHECK_NE(model_index + 1, tab_count());
  if (tab_closing->data().mini && model_index + 1 < tab_count() &&
      !tab_at(model_index + 1)->data().mini) {
    delta += kMiniToNonMiniGap;
  }

  for (int i = model_index + 1; i < tab_count(); ++i) {
    gfx::Rect bounds = ideal_bounds(i);
    bounds.set_x(bounds.x() - delta);
    set_ideal_bounds(i, bounds);
  }

  newtab_button_bounds_.set_x(newtab_button_bounds_.x() - delta);

  PrepareForAnimation();

  tab_closing->set_closing(true);

  // We still need to paint the tab until we actually remove it. Put it in
  // tabs_closing_map_ so we can find it.
  RemoveTabFromViewModel(model_index);

  AnimateToIdealBounds();

  gfx::Rect tab_bounds = tab_closing->bounds();
  tab_bounds.set_width(0);
  bounds_animator_.AnimateViewTo(tab_closing, tab_bounds);

  // Register delegate to do cleanup when done, BoundsAnimator takes
  // ownership of RemoveTabDelegate.
  bounds_animator_.SetAnimationDelegate(
      tab_closing,
      new RemoveTabDelegate(this, tab_closing),
      true);
}

bool TabStrip::IsPointInTab(Tab* tab,
                            const gfx::Point& point_in_tabstrip_coords) {
  gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
  View::ConvertPointToTarget(this, tab, &point_in_tab_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

int TabStrip::GetStartXForNormalTabs() const {
  int mini_tab_count = GetMiniTabCount();
  if (mini_tab_count == 0)
    return 0;
  return mini_tab_count * (Tab::GetMiniWidth() + tab_h_offset()) +
      kMiniToNonMiniGap;
}

Tab* TabStrip::FindTabForEvent(const gfx::Point& point) {
  DCHECK(touch_layout_.get());
  int active_tab_index = touch_layout_->active_index();
  Tab* tab = NULL;
  if (active_tab_index != -1) {
    tab = FindTabForEventFrom(point, active_tab_index, -1);
    if (!tab)
      tab = FindTabForEventFrom(point, active_tab_index + 1, 1);
  } else if (tab_count()) {
    tab = FindTabForEventFrom(point, 0, 1);
  }
  return tab;
}

Tab* TabStrip::FindTabForEventFrom(const gfx::Point& point,
                                   int start,
                                   int delta) {
  // |start| equals tab_count() when there are only pinned tabs.
  if (start == tab_count())
    start += delta;
  for (int i = start; i >= 0 && i < tab_count(); i += delta) {
    if (IsPointInTab(tab_at(i), point))
      return tab_at(i);
  }
  return NULL;
}

std::vector<int> TabStrip::GetTabXCoordinates() {
  std::vector<int> results;
  for (int i = 0; i < tab_count(); ++i)
    results.push_back(ideal_bounds(i).x());
  return results;
}

void TabStrip::SwapLayoutIfNecessary() {
  bool needs_touch = NeedsTouchLayout();
  bool using_touch = touch_layout_.get() != NULL;
  if (needs_touch == using_touch)
    return;

  if (needs_touch) {
    gfx::Size tab_size(Tab::GetMinimumSelectedSize());
    tab_size.set_width(Tab::GetTouchWidth());
    touch_layout_.reset(new StackedTabStripLayout(
                            tab_size,
                            tab_h_offset(),
                            kStackedPadding,
                            kMaxStackedCount,
                            &tabs_));
    touch_layout_->SetWidth(width() - new_tab_button_width());
    // This has to be after SetWidth() as SetWidth() is going to reset the
    // bounds of the mini-tabs (since StackedTabStripLayout doesn't yet know how
    // many mini-tabs there are).
    GenerateIdealBoundsForMiniTabs(NULL);
    touch_layout_->SetXAndMiniCount(GetStartXForNormalTabs(),
                                    GetMiniTabCount());
    touch_layout_->SetActiveIndex(controller_->GetActiveIndex());
  } else {
    touch_layout_.reset();
  }
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

bool TabStrip::NeedsTouchLayout() const {
  if (layout_type_ == TAB_STRIP_LAYOUT_SHRINK)
    return false;

  int mini_tab_count = GetMiniTabCount();
  int normal_count = tab_count() - mini_tab_count;
  if (normal_count <= 1 || normal_count == mini_tab_count)
    return false;
  int x = GetStartXForNormalTabs();
  int available_width = width() - x - new_tab_button_width();
  return (Tab::GetTouchWidth() * normal_count +
          tab_h_offset() * (normal_count - 1)) > available_width;
}

void TabStrip::SetResetToShrinkOnExit(bool value) {
  if (!GetAdjustLayout())
    return;

  if (value && layout_type_ == TAB_STRIP_LAYOUT_SHRINK)
    value = false;  // We're already at TAB_STRIP_LAYOUT_SHRINK.

  if (value == reset_to_shrink_on_exit_)
    return;

  reset_to_shrink_on_exit_ = value;
  // Add an observer so we know when the mouse moves out of the tabstrip.
  if (reset_to_shrink_on_exit_)
    AddMessageLoopObserver();
  else
    RemoveMessageLoopObserver();
}

bool TabStrip::GetAdjustLayout() const {
  if (!adjust_layout_)
    return false;

#if !defined(OS_CHROMEOS)
  if (ui::GetDisplayLayout() != ui::LAYOUT_TOUCH)
    return false;
#endif

  return true;
}
