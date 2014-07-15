// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/toolbar/browser_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/feature_switch.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/metrics.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

using extensions::Extension;

namespace {

// Horizontal spacing between most items in the container, as well as after the
// last item or chevron (if visible).
const int kItemSpacing = ToolbarView::kStandardSpacing;

// Horizontal spacing before the chevron (if visible).
const int kChevronSpacing = kItemSpacing - 2;

// The maximum number of icons to show per row when in overflow mode (showing
// icons in the application menu).
// TODO(devlin): Compute the right number of icons to show, depending on the
//               menu width.
#if defined(OS_LINUX)
const int kIconsPerMenuRow = 8;  // The menu on Linux is wider.
#else
const int kIconsPerMenuRow = 7;
#endif

// A version of MenuButton with almost empty insets to fit properly on the
// toolbar.
class ChevronMenuButton : public views::MenuButton {
 public:
  ChevronMenuButton(views::ButtonListener* listener,
                    const base::string16& text,
                    views::MenuButtonListener* menu_button_listener,
                    bool show_menu_marker)
      : views::MenuButton(listener,
                          text,
                          menu_button_listener,
                          show_menu_marker) {
  }

  virtual ~ChevronMenuButton() {}

  virtual scoped_ptr<views::LabelButtonBorder> CreateDefaultBorder() const
      OVERRIDE {
    // The chevron resource was designed to not have any insets.
    scoped_ptr<views::LabelButtonBorder> border =
        views::MenuButton::CreateDefaultBorder();
    border->set_insets(gfx::Insets());
    return border.Pass();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChevronMenuButton);
};

}  // namespace

// static
bool BrowserActionsContainer::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(
    Browser* browser,
    View* owner_view,
    BrowserActionsContainer* main_container)
    : profile_(browser->profile()),
      browser_(browser),
      owner_view_(owner_view),
      main_container_(main_container),
      popup_(NULL),
      popup_button_(NULL),
      model_(NULL),
      container_width_(0),
      resize_area_(NULL),
      chevron_(NULL),
      overflow_menu_(NULL),
      suppress_chevron_(false),
      resize_amount_(0),
      animation_target_size_(0),
      drop_indicator_position_(-1),
      task_factory_(this),
      show_menu_task_factory_(this) {
  set_id(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  model_ = extensions::ExtensionToolbarModel::Get(browser->profile());
  if (model_)
    model_->AddObserver(this);

  bool overflow_experiment =
      extensions::FeatureSwitch::extension_action_redesign()->IsEnabled();
  DCHECK(!in_overflow_mode() || overflow_experiment);

  if (!in_overflow_mode()) {
    extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
        browser->profile(),
        owner_view->GetFocusManager(),
        extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
        this));

    resize_animation_.reset(new gfx::SlideAnimation(this));
    resize_area_ = new views::ResizeArea(this);
    AddChildView(resize_area_);

    // 'Main' mode doesn't need a chevron overflow when overflow is shown inside
    // the Chrome menu.
    if (!overflow_experiment) {
      chevron_ = new ChevronMenuButton(NULL, base::string16(), this, false);
      chevron_->SetBorder(views::Border::NullBorder());
      chevron_->EnableCanvasFlippingForRTLUI(true);
      chevron_->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS_CHEVRON));
      chevron_->SetVisible(false);
      AddChildView(chevron_);
    }
  }
}

BrowserActionsContainer::~BrowserActionsContainer() {
  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionsContainerDestroyed());

  if (overflow_menu_)
    overflow_menu_->set_observer(NULL);
  if (model_)
    model_->RemoveObserver(this);
  StopShowFolderDropMenuTimer();
  if (popup_)
    popup_->GetWidget()->RemoveObserver(this);
  HidePopup();
  DeleteBrowserActionViews();
}

void BrowserActionsContainer::Init() {
  LoadImages();

  // We wait to set the container width until now so that the chevron images
  // will be loaded.  The width calculation needs to know the chevron size.
  if (model_ &&
      !profile_->GetPrefs()->HasPrefPath(
          extensions::pref_names::kToolbarSize)) {
    // Migration code to the new VisibleIconCount pref.
    // TODO(mpcomplete): remove this after users are upgraded to 5.0.
    int predefined_width = profile_->GetPrefs()->GetInteger(
        extensions::pref_names::kBrowserActionContainerWidth);
    if (predefined_width != 0)
      model_->SetVisibleIconCount(WidthToIconCount(predefined_width));
  }
  if (model_ && model_->extensions_initialized())
    SetContainerWidth();
}

BrowserActionView* BrowserActionsContainer::GetBrowserActionView(
    ExtensionAction* action) {
  for (BrowserActionViews::iterator i(browser_action_views_.begin());
       i != browser_action_views_.end(); ++i) {
    if ((*i)->button()->browser_action() == action)
      return *i;
  }
  return NULL;
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i)
    browser_action_views_[i]->button()->UpdateState();
}

void BrowserActionsContainer::CreateBrowserActionViews() {
  DCHECK(browser_action_views_.empty());
  if (!model_)
    return;

  const extensions::ExtensionList& toolbar_items = model_->toolbar_items();
  for (extensions::ExtensionList::const_iterator i(toolbar_items.begin());
       i != toolbar_items.end(); ++i) {
    if (!ShouldDisplayBrowserAction(i->get()))
      continue;

    BrowserActionView* view = new BrowserActionView(i->get(), browser_, this);
    browser_action_views_.push_back(view);
    AddChildView(view);
  }
}

void BrowserActionsContainer::DeleteBrowserActionViews() {
  HidePopup();
  if (overflow_menu_)
    overflow_menu_->NotifyBrowserActionViewsDeleting();
  STLDeleteElements(&browser_action_views_);
}

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    if (browser_action_views_[i]->visible())
      ++visible_actions;
  }
  return visible_actions;
}

size_t BrowserActionsContainer::VisibleBrowserActionsAfterAnimation() const {
  if (!animating())
    return VisibleBrowserActions();

  return WidthToIconCount(animation_target_size_);
}

void BrowserActionsContainer::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {
  // Global commands are handled by the ExtensionCommandsGlobalRegistry
  // instance.
  DCHECK(!command.global());
  extension_keybinding_registry_->ExecuteCommand(extension->id(),
                                                 command.accelerator());
}

bool BrowserActionsContainer::ShownInsideMenu() const {
  return in_overflow_mode();
}

void BrowserActionsContainer::AddObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserActionsContainer::RemoveObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::Size BrowserActionsContainer::GetPreferredSize() const {
  size_t icon_count = browser_action_views_.size() -
      (in_overflow_mode() ? main_container_->VisibleBrowserActions() : 0);

  // If there are no actions to show, or we are in overflow mode and the main
  // container is already showing them all, then no further work is required.
  if (icon_count == 0)
    return gfx::Size();

  if (in_overflow_mode()) {
    // When in overflow, y is multiline, so the pixel count is IconHeight()
    // times the number of rows needed.
    return gfx::Size(
        IconCountToWidth(kIconsPerMenuRow, false),
        (((icon_count - 1) / kIconsPerMenuRow) + 1) * IconHeight());
  }

  // We calculate the size of the view by taking the current width and
  // subtracting resize_amount_ (the latter represents how far the user is
  // resizing the view or, if animating the snapping, how far to animate it).
  // But we also clamp it to a minimum size and the maximum size, so that the
  // container can never shrink too far or take up more space than it needs.
  // In other words: MinimumNonemptyWidth() < width() - resize < ClampTo(MAX).
  int preferred_width = std::min(
      std::max(MinimumNonemptyWidth(), container_width_ - resize_amount_),
      IconCountToWidth(-1, false));
  return gfx::Size(preferred_width, IconHeight());
}

gfx::Size BrowserActionsContainer::GetMinimumSize() const {
  int min_width = std::min(MinimumNonemptyWidth(), IconCountToWidth(-1, false));
  return gfx::Size(min_width, IconHeight());
}

void BrowserActionsContainer::Layout() {
  if (browser_action_views_.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  if (resize_area_)
    resize_area_->SetBounds(0, 0, kItemSpacing, height());

  // If the icons don't all fit, show the chevron (unless suppressed).
  int max_x = GetPreferredSize().width();
  if ((IconCountToWidth(-1, false) > max_x) && !suppress_chevron_ && chevron_) {
    chevron_->SetVisible(true);
    gfx::Size chevron_size(chevron_->GetPreferredSize());
    max_x -=
        ToolbarView::kStandardSpacing + chevron_size.width() + kChevronSpacing;
    chevron_->SetBounds(
        width() - ToolbarView::kStandardSpacing - chevron_size.width(),
        0,
        chevron_size.width(),
        chevron_size.height());
  } else if (chevron_) {
    chevron_->SetVisible(false);
  }

  // Now draw the icons for the browser actions in the available space.
  int icon_width = IconWidth(false);
  if (in_overflow_mode()) {
    for (size_t i = 0;
         i < main_container_->VisibleBrowserActionsAfterAnimation(); ++i) {
      // Ensure that any browser actions shown in the main view are hidden in
      // the overflow view.
      browser_action_views_[i]->SetVisible(false);
    }

    for (size_t i = main_container_->VisibleBrowserActionsAfterAnimation();
         i < browser_action_views_.size(); ++i) {
      BrowserActionView* view = browser_action_views_[i];
      size_t index = i - main_container_->VisibleBrowserActionsAfterAnimation();
      int row_index = static_cast<int>(index) / kIconsPerMenuRow;
      int x = (index * IconWidth(true)) -
          (row_index * IconWidth(true) * kIconsPerMenuRow);
      gfx::Rect rect_bounds(
          x, IconHeight() * row_index, icon_width, IconHeight());
      view->SetBoundsRect(rect_bounds);
      view->SetVisible(true);
    }
  } else {
    for (BrowserActionViews::const_iterator it = browser_action_views_.begin();
         it < browser_action_views_.end(); ++it) {
      BrowserActionView* view = *it;
      int x = ToolbarView::kStandardSpacing +
          ((it - browser_action_views_.begin()) * IconWidth(true));
      view->SetVisible(x + icon_width <= max_x);
      if (view->visible())
        view->SetBounds(x, 0, icon_width, IconHeight());
    }
  }
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  custom_formats->insert(BrowserActionDragData::GetBrowserActionCustomFormat());

  return true;
}

bool BrowserActionsContainer::AreDropTypesRequired() {
  return true;
}

bool BrowserActionsContainer::CanDrop(const OSExchangeData& data) {
  BrowserActionDragData drop_data;
  return drop_data.Read(data) ? drop_data.IsFromProfile(profile_) : false;
}

void BrowserActionsContainer::OnDragEntered(
    const ui::DropTargetEvent& event) {
}

int BrowserActionsContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  // First check if we are above the chevron (overflow) menu.
  if (GetEventHandlerForPoint(event.location()) == chevron_) {
    if (!show_menu_task_factory_.HasWeakPtrs() && !overflow_menu_)
      StartShowFolderDropMenuTimer();
    return ui::DragDropTypes::DRAG_MOVE;
  }
  StopShowFolderDropMenuTimer();

  // TODO(devlin): This calculation needs to take 'overflow' mode into account
  // once the wrench menu becomes a drag target for browser action icons.

  // Figure out where to display the indicator.  This is a complex calculation:

  // First, we figure out how much space is to the left of the icon area, so we
  // can calculate the true offset into the icon area.
  int width_before_icons = ToolbarView::kStandardSpacing;
  if (chevron_ && base::i18n::IsRTL()) {
    width_before_icons +=
        chevron_->GetPreferredSize().width() + kChevronSpacing;
  }
  int offset_into_icon_area = event.x() - width_before_icons;

  // Next, we determine which icon to place the indicator in front of.  We want
  // to place the indicator in front of icon n when the cursor is between the
  // midpoints of icons (n - 1) and n.  To do this we take the offset into the
  // icon area and transform it as follows:
  //
  // Real icon area:
  //   0   a     *  b        c
  //   |   |        |        |
  //   |[IC|ON]  [IC|ON]  [IC|ON]
  // We want to be before icon 0 for 0 < x <= a, icon 1 for a < x <= b, etc.
  // Here the "*" represents the offset into the icon area, and since it's
  // between a and b, we want to return "1".
  //
  // Transformed "icon area":
  //   0        a     *  b        c
  //   |        |        |        |
  //   |[ICON]  |[ICON]  |[ICON]  |
  // If we shift both our offset and our divider points later by half an icon
  // plus one spacing unit, then it becomes very easy to calculate how many
  // divider points we've passed, because they're the multiples of "one icon
  // plus padding".
  int before_icon_unclamped = (offset_into_icon_area + (IconWidth(false) / 2) +
      kItemSpacing) / IconWidth(true);

  // Because the user can drag outside the container bounds, we need to clamp to
  // the valid range.  Note that the maximum allowable value is (num icons), not
  // (num icons - 1), because we represent the indicator being past the last
  // icon as being "before the (last + 1) icon".
  int before_icon = std::min(std::max(before_icon_unclamped, 0),
                             static_cast<int>(VisibleBrowserActions()));

  // Now we convert back to a pixel offset into the container.  We want to place
  // the center of the drop indicator at the midpoint of the space before our
  // chosen icon.
  SetDropIndicator(width_before_icons + (before_icon * IconWidth(true)) -
      (kItemSpacing / 2));

  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
  StopShowFolderDropMenuTimer();
  drop_indicator_position_ = -1;
  SchedulePaint();
}

int BrowserActionsContainer::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  // Make sure we have the same view as we started with.
  DCHECK_EQ(browser_action_views_[data.index()]->button()->extension()->id(),
            data.id());
  DCHECK(model_);

  size_t i = 0;
  for (; i < browser_action_views_.size(); ++i) {
    int view_x = browser_action_views_[i]->GetMirroredBounds().x();
    if (!browser_action_views_[i]->visible() ||
        (base::i18n::IsRTL() ? (view_x < drop_indicator_position_) :
            (view_x >= drop_indicator_position_))) {
      // We have reached the end of the visible icons or found one that has a
      // higher x position than the drop point.
      break;
    }
  }

  // |i| now points to the item to the right of the drop indicator*, which is
  // correct when dragging an icon to the left. When dragging to the right,
  // however, we want the icon being dragged to get the index of the item to
  // the left of the drop indicator, so we subtract one.
  // * Well, it can also point to the end, but not when dragging to the left. :)
  if (i > data.index())
    --i;

  if (profile_->IsOffTheRecord())
    i = model_->IncognitoIndexToOriginal(i);

  model_->MoveBrowserAction(
      browser_action_views_[data.index()]->button()->extension(), i);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS);
}

void BrowserActionsContainer::OnMenuButtonClicked(views::View* source,
                                                  const gfx::Point& point) {
  if (source == chevron_) {
    overflow_menu_ =
        new BrowserActionOverflowMenuController(this,
                                                browser_,
                                                chevron_,
                                                browser_action_views_,
                                                VisibleBrowserActions(),
                                                false);
    overflow_menu_->set_observer(this);
    overflow_menu_->RunMenu(GetWidget());
  }
}

void BrowserActionsContainer::WriteDragDataForView(View* sender,
                                                   const gfx::Point& press_pt,
                                                   OSExchangeData* data) {
  DCHECK(data);

  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionButton* button = browser_action_views_[i]->button();
    if (button == sender) {
      // Set the dragging image for the icon.
      gfx::ImageSkia badge(browser_action_views_[i]->GetIconWithBadge());
      drag_utils::SetDragImageOnDataObject(badge,
                                           press_pt.OffsetFromOrigin(),
                                           data);

      // Fill in the remaining info.
      BrowserActionDragData drag_data(
          browser_action_views_[i]->button()->extension()->id(), i);
      drag_data.Write(profile_, data);
      break;
    }
  }
}

int BrowserActionsContainer::GetDragOperationsForView(View* sender,
                                                      const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDragForView(View* sender,
                                                  const gfx::Point& press_pt,
                                                  const gfx::Point& p) {
  // We don't allow dragging while we're highlighting.
  return !model_->is_highlighting();
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  if (!done_resizing) {
    resize_amount_ = resize_amount;
    OnBrowserActionVisibilityChanged();
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  int max_width = IconCountToWidth(-1, false);
  container_width_ =
      std::min(std::max(0, container_width_ - resize_amount), max_width);
  SaveDesiredSizeAndAnimate(gfx::Tween::EASE_OUT,
                            WidthToIconCount(container_width_));
}

void BrowserActionsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  resize_amount_ = static_cast<int>(resize_animation_->GetCurrentValue() *
      (container_width_ - animation_target_size_));
  OnBrowserActionVisibilityChanged();
}

void BrowserActionsContainer::AnimationEnded(const gfx::Animation* animation) {
  container_width_ = animation_target_size_;
  animation_target_size_ = 0;
  resize_amount_ = 0;
  suppress_chevron_ = false;
  OnBrowserActionVisibilityChanged();

  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionsContainerAnimationEnded());
}

void BrowserActionsContainer::NotifyMenuDeleted(
    BrowserActionOverflowMenuController* controller) {
  DCHECK_EQ(overflow_menu_, controller);
  overflow_menu_ = NULL;
}

void BrowserActionsContainer::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(popup_->GetWidget(), widget);
  popup_->GetWidget()->RemoveObserver(this);
  popup_ = NULL;
  // |popup_button_| is NULL if the extension has been removed.
  if (popup_button_) {
    popup_button_->SetButtonNotPushed();
    popup_button_ = NULL;
  }
}

void BrowserActionsContainer::InspectPopup(ExtensionAction* action) {
  BrowserActionView* view = GetBrowserActionView(action);
  ShowPopup(view->button(), ExtensionPopup::SHOW_AND_INSPECT, true);
}

int BrowserActionsContainer::GetCurrentTabId() const {
  content::WebContents* active_tab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!active_tab)
    return -1;

  return SessionTabHelper::FromWebContents(active_tab)->session_id().id();
}

void BrowserActionsContainer::OnBrowserActionExecuted(
    BrowserActionButton* button) {
  ShowPopup(button, ExtensionPopup::SHOW, true);
}

void BrowserActionsContainer::OnBrowserActionVisibilityChanged() {
  SetVisible(!browser_action_views_.empty());
  if (owner_view_) {
    owner_view_->Layout();
    owner_view_->SchedulePaint();
  }
}

extensions::ActiveTabPermissionGranter*
    BrowserActionsContainer::GetActiveTabPermissionGranter() {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return NULL;
  return extensions::TabHelper::FromWebContents(web_contents)->
      active_tab_permission_granter();
}

void BrowserActionsContainer::MoveBrowserAction(const std::string& extension_id,
                                                size_t new_index) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service) {
    const Extension* extension = service->GetExtensionById(extension_id, false);
    model_->MoveBrowserAction(extension, new_index);
    SchedulePaint();
  }
}

bool BrowserActionsContainer::ShowPopup(const extensions::Extension* extension,
                                        bool should_grant) {
  // Do not override other popups and only show in active window. The window
  // must also have a toolbar, otherwise it should not be showing popups.
  // TODO(justinlin): Remove toolbar check when http://crbug.com/308645 is
  // fixed.
  if (popup_ ||
      !browser_->window()->IsActive() ||
      !browser_->window()->IsToolbarVisible()) {
    return false;
  }

  for (BrowserActionViews::iterator it = browser_action_views_.begin();
       it != browser_action_views_.end(); ++it) {
    BrowserActionButton* button = (*it)->button();
    if (button && button->extension() == extension)
      return ShowPopup(button, ExtensionPopup::SHOW, should_grant);
  }
  return false;
}

void BrowserActionsContainer::HidePopup() {
  // Remove this as an observer and clear |popup_| and |popup_button_| here,
  // since we might change them before OnWidgetDestroying() gets called.
  if (popup_) {
    popup_->GetWidget()->RemoveObserver(this);
    popup_->GetWidget()->Close();
    popup_ = NULL;
  }
  if (popup_button_) {
    popup_button_->SetButtonNotPushed();
    popup_button_ = NULL;
  }
}

void BrowserActionsContainer::TestExecuteBrowserAction(int index) {
  BrowserActionButton* button = browser_action_views_[index]->button();
  OnBrowserActionExecuted(button);
}

void BrowserActionsContainer::TestSetIconVisibilityCount(size_t icons) {
  model_->SetVisibleIconCount(icons);
  chevron_->SetVisible(icons < browser_action_views_.size());
  container_width_ = IconCountToWidth(icons, chevron_->visible());
  Layout();
  SchedulePaint();
}

void BrowserActionsContainer::OnPaint(gfx::Canvas* canvas) {
  // If the views haven't been initialized yet, wait for the next call to
  // paint (one will be triggered by entering highlight mode).
  if (model_->is_highlighting() && !browser_action_views_.empty()) {
    views::Painter::PaintPainterAt(
        canvas, highlight_painter_.get(), GetLocalBounds());
  }

  // TODO(sky/glen): Instead of using a drop indicator, animate the icons while
  // dragging (like we do for tab dragging).
  if (drop_indicator_position_ > -1) {
    // The two-pixel width drop indicator.
    static const int kDropIndicatorWidth = 2;
    gfx::Rect indicator_bounds(
        drop_indicator_position_ - (kDropIndicatorWidth / 2),
        0,
        kDropIndicatorWidth,
        height());

    // Color of the drop indicator.
    static const SkColor kDropIndicatorColor = SK_ColorBLACK;
    canvas->FillRect(indicator_bounds, kDropIndicatorColor);
  }
}

void BrowserActionsContainer::OnThemeChanged() {
  LoadImages();
}

void BrowserActionsContainer::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // No extensions (e.g., incognito).
  if (!model_)
    return;

  if (details.is_add && details.child == this) {
    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because AddBrowserAction
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    CreateBrowserActionViews();
  }
}

// static
int BrowserActionsContainer::IconWidth(bool include_padding) {
  static bool initialized = false;
  static int icon_width = 0;
  if (!initialized) {
    initialized = true;
    icon_width = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_BROWSER_ACTION)->width();
  }
  return icon_width + (include_padding ? kItemSpacing : 0);
}

// static
int BrowserActionsContainer::IconHeight() {
  static bool initialized = false;
  static int icon_height = 0;
  if (!initialized) {
    initialized = true;
    icon_height = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_BROWSER_ACTION)->height();
  }
  return icon_height;
}

void BrowserActionsContainer::BrowserActionAdded(const Extension* extension,
                                                 int index) {
#if defined(DEBUG)
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    DCHECK(browser_action_views_[i]->button()->extension() != extension) <<
           "Asked to add a browser action view for an extension that already "
           "exists.";
  }
#endif
  CloseOverflowMenu();

  if (!ShouldDisplayBrowserAction(extension))
    return;

  size_t visible_actions = VisibleBrowserActionsAfterAnimation();

  // Add the new browser action to the vector and the view hierarchy.
  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);
  BrowserActionView* view = new BrowserActionView(extension, browser_, this);
  browser_action_views_.insert(browser_action_views_.begin() + index, view);
  AddChildViewAt(view, index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // Enlarge the container if it was already at maximum size and we're not in
  // the middle of upgrading.
  if ((model_->GetVisibleIconCount() < 0) &&
      !extensions::ExtensionSystem::Get(profile_)->runtime_data()->
          IsBeingUpgraded(extension)) {
    suppress_chevron_ = true;
    SaveDesiredSizeAndAnimate(gfx::Tween::LINEAR, visible_actions + 1);
  } else {
    // Just redraw the (possibly modified) visible icon set.
    OnBrowserActionVisibilityChanged();
  }
}

void BrowserActionsContainer::BrowserActionRemoved(const Extension* extension) {
  CloseOverflowMenu();

  if (popup_ && popup_->host()->extension() == extension)
    HidePopup();

  size_t visible_actions = VisibleBrowserActionsAfterAnimation();
  for (BrowserActionViews::iterator i(browser_action_views_.begin());
       i != browser_action_views_.end(); ++i) {
    if ((*i)->button()->extension() == extension) {
      delete *i;
      browser_action_views_.erase(i);

      // If the extension is being upgraded we don't want the bar to shrink
      // because the icon is just going to get re-added to the same location.
      if (extensions::ExtensionSystem::Get(profile_)->runtime_data()->
              IsBeingUpgraded(extension))
        return;

      if (browser_action_views_.size() > visible_actions) {
        // If we have more icons than we can show, then we must not be changing
        // the container size (since we either removed an icon from the main
        // area and one from the overflow list will have shifted in, or we
        // removed an entry directly from the overflow list).
        OnBrowserActionVisibilityChanged();
      } else {
        // Either we went from overflow to no-overflow, or we shrunk the no-
        // overflow container by 1.  Either way the size changed, so animate.
        if (chevron_)
          chevron_->SetVisible(false);
        SaveDesiredSizeAndAnimate(gfx::Tween::EASE_OUT,
                                  browser_action_views_.size());
      }
      return;  // We have found the action to remove, bail out.
    }
  }
}

void BrowserActionsContainer::BrowserActionMoved(const Extension* extension,
                                                 int index) {
  if (!ShouldDisplayBrowserAction(extension))
    return;

  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);

  DCHECK(index >= 0 && index < static_cast<int>(browser_action_views_.size()));

  DeleteBrowserActionViews();
  CreateBrowserActionViews();
  Layout();
  SchedulePaint();
}

bool BrowserActionsContainer::BrowserActionShowPopup(
    const extensions::Extension* extension) {
  return ShowPopup(extension, false);
}

void BrowserActionsContainer::VisibleCountChanged() {
  SetContainerWidth();
}

void BrowserActionsContainer::HighlightModeChanged(bool is_highlighting) {
  // The visual highlighting is done in OnPaint(). It's a bit of a pain that
  // we delete and recreate everything here, but that's how it's done in
  // BrowserActionMoved(), too. If we want to optimize it, we could move the
  // existing icons, instead of deleting it all.
  DeleteBrowserActionViews();
  CreateBrowserActionViews();
  SaveDesiredSizeAndAnimate(gfx::Tween::LINEAR, browser_action_views_.size());
}

void BrowserActionsContainer::LoadImages() {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (!tp || !chevron_)
    return;

  chevron_->SetImage(views::Button::STATE_NORMAL,
                     *tp->GetImageSkiaNamed(IDR_BROWSER_ACTIONS_OVERFLOW));

  const int kImages[] = IMAGE_GRID(IDR_DEVELOPER_MODE_HIGHLIGHT);
  highlight_painter_.reset(views::Painter::CreateImageGridPainter(kImages));
}

void BrowserActionsContainer::SetContainerWidth() {
  // The slave only draws the overflow (what isn't visible in the other
  // container).
  int visible_actions = in_overflow_mode() ?
      model_->toolbar_items().size() - model_->GetVisibleIconCount() :
      model_->GetVisibleIconCount();
  if (visible_actions < 0)  // All icons should be visible.
    visible_actions = model_->toolbar_items().size();
  if (chevron_) {
    chevron_->SetVisible(
      static_cast<size_t>(visible_actions) < model_->toolbar_items().size());
  }
  container_width_ =
      IconCountToWidth(visible_actions, chevron_ && chevron_->visible());
}

void BrowserActionsContainer::CloseOverflowMenu() {
  if (overflow_menu_)
    overflow_menu_->CancelMenu();
}

void BrowserActionsContainer::StopShowFolderDropMenuTimer() {
  show_menu_task_factory_.InvalidateWeakPtrs();
}

void BrowserActionsContainer::StartShowFolderDropMenuTimer() {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BrowserActionsContainer::ShowDropFolder,
                 show_menu_task_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
}

void BrowserActionsContainer::ShowDropFolder() {
  DCHECK(!overflow_menu_);
  SetDropIndicator(-1);
  overflow_menu_ =
      new BrowserActionOverflowMenuController(this,
                                              browser_,
                                              chevron_,
                                              browser_action_views_,
                                              VisibleBrowserActions(),
                                              true);
  overflow_menu_->set_observer(this);
  overflow_menu_->RunMenu(GetWidget());
}

void BrowserActionsContainer::SetDropIndicator(int x_pos) {
  if (drop_indicator_position_ != x_pos) {
    drop_indicator_position_ = x_pos;
    SchedulePaint();
  }
}

int BrowserActionsContainer::IconCountToWidth(int icons,
                                              bool display_chevron) const {
  if (icons < 0)
    icons = browser_action_views_.size();
  if ((icons == 0) && !display_chevron)
    return ToolbarView::kStandardSpacing;
  int icons_size =
      (icons == 0) ? 0 : ((icons * IconWidth(true)) - kItemSpacing);
  int chevron_size = chevron_ && display_chevron ?
      (kChevronSpacing + chevron_->GetPreferredSize().width()) : 0;
  return ToolbarView::kStandardSpacing + icons_size + chevron_size +
      ToolbarView::kStandardSpacing;
}

size_t BrowserActionsContainer::WidthToIconCount(int pixels) const {
  // Check for widths large enough to show the entire icon set.
  if (pixels >= IconCountToWidth(-1, false))
    return browser_action_views_.size();

  // We need to reserve space for the resize area, chevron, and the spacing on
  // either side of the chevron.
  int available_space = pixels - ToolbarView::kStandardSpacing -
      (chevron_ ? chevron_->GetPreferredSize().width() : 0) -
      kChevronSpacing - ToolbarView::kStandardSpacing;
  // Now we add an extra between-item padding value so the space can be divided
  // evenly by (size of icon with padding).
  return static_cast<size_t>(
      std::max(0, available_space + kItemSpacing) / IconWidth(true));
}

int BrowserActionsContainer::MinimumNonemptyWidth() const {
  if (!chevron_)
    return ToolbarView::kStandardSpacing;
  return (ToolbarView::kStandardSpacing * 2) + kChevronSpacing +
      chevron_->GetPreferredSize().width();
}

void BrowserActionsContainer::SaveDesiredSizeAndAnimate(
    gfx::Tween::Type tween_type,
    size_t num_visible_icons) {
  // Save off the desired number of visible icons.  We do this now instead of at
  // the end of the animation so that even if the browser is shut down while
  // animating, the right value will be restored on next run.
  // NOTE: Don't save the icon count in incognito because there may be fewer
  // icons in that mode. The result is that the container in a normal window is
  // always at least as wide as in an incognito window.
  if (!profile_->IsOffTheRecord())
    model_->SetVisibleIconCount(num_visible_icons);
  int target_size = IconCountToWidth(num_visible_icons,
      num_visible_icons < browser_action_views_.size());
  if (resize_animation_ && !disable_animations_during_testing_) {
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_size;
    resize_animation_->Show();
  } else {
    animation_target_size_ = target_size;
    AnimationEnded(resize_animation_.get());
  }
}

bool BrowserActionsContainer::ShouldDisplayBrowserAction(
    const Extension* extension) {
  // Only display incognito-enabled extensions while in incognito mode.
  return !profile_->IsOffTheRecord() ||
      extensions::util::IsIncognitoEnabled(extension->id(), profile_);
}

bool BrowserActionsContainer::ShowPopup(
    BrowserActionButton* button,
    ExtensionPopup::ShowAction show_action,
    bool should_grant) {
  const Extension* extension = button->extension();
  GURL popup_url;
  if (model_->ExecuteBrowserAction(
          extension, browser_, &popup_url, should_grant) !=
      extensions::ExtensionToolbarModel::ACTION_SHOW_POPUP) {
    return false;
  }

  // If we're showing the same popup, just hide it and return.
  bool same_showing = popup_ && button == popup_button_;

  // Always hide the current popup, even if it's not the same.
  // Only one popup should be visible at a time.
  HidePopup();

  if (same_showing)
    return false;

  // We can get the execute event for browser actions that are not visible,
  // since buttons can be activated from the overflow menu (chevron). In that
  // case we show the popup as originating from the chevron.
  View* reference_view = button->parent()->visible() ? button : chevron_;
  popup_ = ExtensionPopup::ShowPopup(popup_url, browser_, reference_view,
                                     views::BubbleBorder::TOP_RIGHT,
                                     show_action);
  popup_->GetWidget()->AddObserver(this);
  popup_button_ = button;

  // Only set button as pushed if it was triggered by a user click.
  if (should_grant)
    popup_button_->SetButtonPushed();
  return true;
}
