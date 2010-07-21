// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_actions_container.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/detachable_toolbar_view.h"
#include "chrome/browser/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/canvas_skia.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/drag_utils.h"
#include "views/window/window.h"

#include "grit/theme_resources.h"

namespace {

// The size (both dimensions) of the buttons for page actions.
static const int kButtonSize = 29;

// The padding between the browser actions and the OmniBox/page menu.
static const int kHorizontalPadding = 4;
static const int kHorizontalPaddingRtl = 8;

// The padding between browser action buttons. Visually, the actual number of
// empty (non-drawing) pixels is this value + 2 when adjacent browser icons
// use their maximum allowed size.
static const int kBrowserActionButtonPadding = 3;

// This is the same value from toolbar.cc. We position the browser actions
// container flush with the edges of the toolbar as a special case so that we
// can draw the badge outside the visual bounds of the container.
static const int kControlVertOffset = 6;

// The margin between the divider and the chrome menu buttons.
static const int kDividerHorizontalMargin = 2;

// The padding above and below the divider.
static const int kDividerVerticalPadding = 9;

// The margin above the chevron.
static const int kChevronTopMargin = 9;

// The margin to the right of the chevron.
static const int kChevronRightMargin = 4;

// Extra hit-area for the resize gripper.
static const int kExtraResizeArea = 4;

// Width of the drop indicator.
static const int kDropIndicatorWidth = 2;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

// The x offset for the drop indicator (how much we shift it by).
static const int kDropIndicatorOffsetLtr = 3;
static const int kDropIndicatorOffsetRtl = 9;

}  // namespace.

// Static.
bool BrowserActionsContainer::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(Extension* extension,
                                         BrowserActionsContainer* panel)
    : ALLOW_THIS_IN_INITIALIZER_LIST(MenuButton(this, L"", NULL, false)),
      browser_action_(extension->browser_action()),
      extension_(extension),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      showing_context_menu_(false),
      panel_(panel) {
  set_alignment(TextButton::ALIGN_CENTER);

  // No UpdateState() here because View hierarchy not setup yet. Our parent
  // should call UpdateState() after creation.

  registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                 Source<ExtensionAction>(browser_action_));

  // The Browser Action API does not allow the default icon path to be changed
  // at runtime, so we can load this now and cache it.
  std::string relative_path = browser_action_->default_icon_path();
  if (relative_path.empty())
    return;

  // This is a bit sketchy because if ImageLoadingTracker calls
  // ::OnImageLoaded() before our creator appends up to the view hierarchy, we
  // will crash. But since we know that ImageLoadingTracker is asynchronous,
  // this should be OK. And doing this in the constructor means that we don't
  // have to protect against it getting done multiple times.
  tracker_.LoadImage(extension, extension->GetResource(relative_path),
                     gfx::Size(Extension::kBrowserActionIconMaxSize,
                               Extension::kBrowserActionIconMaxSize),
                     ImageLoadingTracker::DONT_CACHE);
}

void BrowserActionButton::Destroy() {
  if (showing_context_menu_) {
    context_menu_menu_->CancelMenu();
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

gfx::Insets BrowserActionButton::GetInsets() const {
  static gfx::Insets zero_inset;
  return zero_inset;
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
    const views::Event& event) {
  panel_->OnBrowserActionExecuted(this, false);  // inspect_with_devtools
}

void BrowserActionButton::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  if (image)
    default_icon_ = *image;

  // Call back to UpdateState() because a more specific icon might have been set
  // while the load was outstanding.
  UpdateState();
}

void BrowserActionButton::UpdateState() {
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  SkBitmap image = browser_action()->GetIcon(tab_id);
  if (!image.isNull())
    SetIcon(image);
  else if (!default_icon_.isNull())
    SetIcon(default_icon_);

  // If the browser action name is empty, show the extension name instead.
  std::wstring name = UTF8ToWide(browser_action()->GetTitle(tab_id));
  if (name.empty())
    name = UTF8ToWide(extension()->name());
  SetTooltipText(name);
  SetAccessibleName(name);
  GetParent()->SchedulePaint();
}

void BrowserActionButton::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED) {
    UpdateState();
    // The browser action may have become visible/hidden so we need to make
    // sure the state gets updated.
    panel_->OnBrowserActionVisibilityChanged();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

bool BrowserActionButton::IsPopup() {
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0) {
    NOTREACHED() << "Button is not on a specific tab.";
    return false;
  }
  return browser_action_->HasPopup(tab_id);
}

GURL BrowserActionButton::GetPopupUrl() {
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0) {
    NOTREACHED() << "Button is not on a specific tab.";
    GURL empty_url;
    return empty_url;
  }
  return browser_action_->GetPopupUrl(tab_id);
}

bool BrowserActionButton::Activate() {
  if (IsPopup()) {
    panel_->OnBrowserActionExecuted(this, false);  // |inspect_with_devtools|.

    // TODO(erikkay): Run a nested modal loop while the mouse is down to
    // enable menu-like drag-select behavior.

    // The return value of this method is returned via OnMousePressed.
    // We need to return false here since we're handing off focus to another
    // widget/view, and true will grab it right back and try to send events
    // to us.
    return false;
  }
  return true;
}

bool BrowserActionButton::OnMousePressed(const views::MouseEvent& e) {
  if (e.IsRightMouseButton()) {
    // Get the top left point of this button in screen coordinates.
    gfx::Point point = gfx::Point(0, 0);
    ConvertPointToScreen(this, &point);

    // Make the menu appear below the button.
    point.Offset(0, height());

    ShowContextMenu(point, true);
    return false;
  } else if (IsPopup()) {
    return MenuButton::OnMousePressed(e);
  }
  return TextButton::OnMousePressed(e);
}

void BrowserActionButton::OnMouseReleased(const views::MouseEvent& e,
                                          bool canceled) {
  if (IsPopup() || showing_context_menu_) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(e, canceled);
  } else {
    TextButton::OnMouseReleased(e, canceled);
  }
}

bool BrowserActionButton::OnKeyReleased(const views::KeyEvent& e) {
  if (IsPopup())
    return MenuButton::OnKeyReleased(e);
  return TextButton::OnKeyReleased(e);
}

void BrowserActionButton::OnMouseExited(const views::MouseEvent& e) {
  if (IsPopup() || showing_context_menu_)
    MenuButton::OnMouseExited(e);
  else
    TextButton::OnMouseExited(e);
}

void BrowserActionButton::ShowContextMenu(const gfx::Point& p,
                                          bool is_mouse_gesture) {
  showing_context_menu_ = true;
  SetButtonPushed();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  context_menu_contents_ = new ExtensionContextMenuModel(
      extension(), panel_->browser(), panel_);
  context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  context_menu_menu_->RunContextMenuAt(p);

  SetButtonNotPushed();
  showing_context_menu_ = false;
}

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::BS_PUSHED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::BS_NORMAL);
  menu_visible_ = false;
}

BrowserActionButton::~BrowserActionButton() {
}


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

BrowserActionView::BrowserActionView(Extension* extension,
                                     BrowserActionsContainer* panel)
    : panel_(panel) {
  button_ = new BrowserActionButton(extension, panel);
  button_->SetDragController(panel_);
  AddChildView(button_);
  button_->UpdateState();
  SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION));
}

BrowserActionView::~BrowserActionView() {
  RemoveChildView(button_);
  button_->Destroy();
}

gfx::Canvas* BrowserActionView::GetIconWithBadge() {
  int tab_id = panel_->GetCurrentTabId();

  SkBitmap icon = button_->extension()->browser_action()->GetIcon(tab_id);
  if (icon.isNull())
    icon = button_->default_icon();

  gfx::Canvas* canvas =
      new gfx::CanvasSkia(icon.width(), icon.height(), false);
  canvas->DrawBitmapInt(icon, 0, 0);

  if (tab_id >= 0) {
    gfx::Rect bounds =
        gfx::Rect(icon.width(), icon.height() + kControlVertOffset);
    button_->extension()->browser_action()->PaintBadge(canvas,
                                                       bounds, tab_id);
  }

  return canvas;
}

bool BrowserActionView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);
  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}

void BrowserActionView::Layout() {
  button_->SetBounds(0, kControlVertOffset, width(), kButtonSize);
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  ExtensionAction* action = button()->browser_action();
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  action->PaintBadge(canvas, gfx::Rect(width(), height()), tab_id);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(
    Browser* browser, View* owner_view)
    : profile_(browser->profile()),
      browser_(browser),
      owner_view_(owner_view),
      popup_(NULL),
      popup_button_(NULL),
      model_(NULL),
      resize_gripper_(NULL),
      chevron_(NULL),
      overflow_menu_(NULL),
      suppress_chevron_(false),
      resize_amount_(0),
      animation_target_size_(0),
      drop_indicator_position_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_menu_task_factory_(this)) {
  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  if (profile_->GetExtensionsService()) {
    model_ = profile_->GetExtensionsService()->toolbar_model();
    model_->AddObserver(this);
  }
  resize_animation_.reset(new SlideAnimation(this));
  resize_gripper_ = new views::ResizeGripper(this);
  resize_gripper_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_SEPARATOR));
  resize_gripper_->SetVisible(false);
  AddChildView(resize_gripper_);

  // TODO(glen): Come up with a new bitmap for the chevron.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* chevron_image = rb.GetBitmapNamed(IDR_BOOKMARK_BAR_CHEVRONS);
  chevron_ = new views::MenuButton(NULL, std::wstring(), this, false);
  chevron_->SetVisible(false);
  chevron_->SetIcon(*chevron_image);
  chevron_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_EXTENSIONS_CHEVRON));
  // Chevron contains >> that should point left in LTR locales.
  chevron_->EnableCanvasFlippingForRTLUI(true);
  AddChildView(chevron_);

  if (!profile_->GetPrefs()->HasPrefPath(prefs::kExtensionToolbarSize)) {
    // Migration code to the new VisibleIconCount pref.
    // TODO(mpcomplete): remove this after users are upgraded to 5.0.
    int predefined_width =
        profile_->GetPrefs()->GetInteger(prefs::kBrowserActionContainerWidth);
    if (predefined_width != 0) {
      int icon_width = (kButtonSize + kBrowserActionButtonPadding);
      if (model_) {
        model_->SetVisibleIconCount(
            (predefined_width - WidthOfNonIconArea()) / icon_width);
      }
    }
  }

  if (model_ && model_->extensions_initialized())
    SetContainerWidth();

  SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_EXTENSIONS));
}

BrowserActionsContainer::~BrowserActionsContainer() {
  if (model_)
    model_->RemoveObserver(this);
  StopShowFolderDropMenuTimer();
  HidePopup();
  DeleteBrowserActionViews();
}

// Static.
void BrowserActionsContainer::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kBrowserActionContainerWidth, 0);
}

int BrowserActionsContainer::GetCurrentTabId() const {
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  if (!tab_contents)
    return -1;

  return tab_contents->controller().session_id().id();
}

BrowserActionView* BrowserActionsContainer::GetBrowserActionView(
    ExtensionAction* action) {
  for (BrowserActionViews::iterator iter =
       browser_action_views_.begin(); iter != browser_action_views_.end();
       ++iter) {
    if ((*iter)->button()->browser_action() == action)
      return *iter;
  }

  return NULL;
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i)
    browser_action_views_[i]->button()->UpdateState();
}

void BrowserActionsContainer::CloseOverflowMenu() {
  if (overflow_menu_)
    overflow_menu_->CancelMenu();
}

void BrowserActionsContainer::StopShowFolderDropMenuTimer() {
  show_menu_task_factory_.RevokeAll();
}

void BrowserActionsContainer::StartShowFolderDropMenuTimer() {
  int delay = View::GetMenuShowDelay();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      show_menu_task_factory_.NewRunnableMethod(
          &BrowserActionsContainer::ShowDropFolder),
      delay);
}

void BrowserActionsContainer::ShowDropFolder() {
  DCHECK(!overflow_menu_);
  SetDropIndicator(-1);
  overflow_menu_ = new BrowserActionOverflowMenuController(
      this, chevron_, browser_action_views_, VisibleBrowserActions());
  overflow_menu_->set_observer(this);
  overflow_menu_->RunMenu(GetWindow()->GetNativeWindow(), true);
}

void BrowserActionsContainer::SetDropIndicator(int x_pos) {
  if (drop_indicator_position_ != x_pos) {
    drop_indicator_position_ = x_pos;
    SchedulePaint();
  }
}

void BrowserActionsContainer::CreateBrowserActionViews() {
  DCHECK(browser_action_views_.empty());
  if (!model_)
    return;

  for (ExtensionList::iterator iter = model_->begin();
       iter != model_->end(); ++iter) {
    if (!ShouldDisplayBrowserAction(*iter))
      continue;

    BrowserActionView* view = new BrowserActionView(*iter, this);
    browser_action_views_.push_back(view);
    AddChildView(view);
  }
}

void BrowserActionsContainer::DeleteBrowserActionViews() {
  if (!browser_action_views_.empty()) {
    for (size_t i = 0; i < browser_action_views_.size(); ++i)
      RemoveChildView(browser_action_views_[i]);
    STLDeleteContainerPointers(browser_action_views_.begin(),
                               browser_action_views_.end());
    browser_action_views_.clear();
  }
}

void BrowserActionsContainer::OnBrowserActionVisibilityChanged() {
  SetVisible(browser_action_views_.size() > 0);
  resize_gripper_->SetVisible(browser_action_views_.size() > 0);

  owner_view_->Layout();
  owner_view_->SchedulePaint();
}

void BrowserActionsContainer::HidePopup() {
  if (popup_)
    popup_->Close();
}

void BrowserActionsContainer::TestExecuteBrowserAction(int index) {
  BrowserActionButton* button = browser_action_views_[index]->button();
  OnBrowserActionExecuted(button, false);  // |inspect_with_devtools|.
}

void BrowserActionsContainer::TestSetIconVisibilityCount(size_t icons) {
  chevron_->SetVisible(icons < browser_action_views_.size());
  container_size_.set_width(IconCountToWidth(icons));
  Layout();
  SchedulePaint();
}

void BrowserActionsContainer::OnBrowserActionExecuted(
    BrowserActionButton* button, bool inspect_with_devtools) {
  ExtensionAction* browser_action = button->browser_action();

  // Popups just display.  No notification to the extension.
  // TODO(erikkay): should there be?
  if (button->IsPopup()) {
    // If we're showing the same popup, just hide it and return.
    bool same_showing = popup_ && button == popup_button_;

    // Always hide the current popup, even if it's not the same.
    // Only one popup should be visible at a time.
    HidePopup();

    if (same_showing)
      return;

    // We can get the execute event for browser actions that are not visible,
    // since buttons can be activated from the overflow menu (chevron). In that
    // case we show the popup as originating from the chevron.
    View* reference_view = button->GetParent()->IsVisible() ? button : chevron_;
    gfx::Point origin;
    View::ConvertPointToScreen(reference_view, &origin);
    gfx::Rect rect = reference_view->bounds();
    rect.set_x(origin.x());
    rect.set_y(origin.y());

    gfx::NativeWindow frame_window =
        browser_->window()->GetNativeHandle();
    BubbleBorder::ArrowLocation arrow_location = base::i18n::IsRTL() ?
        BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;

    popup_ = ExtensionPopup::Show(button->GetPopupUrl(), browser_,
        browser_->profile(), frame_window, rect, arrow_location,
        true,  // Activate the popup window.
        inspect_with_devtools,
        ExtensionPopup::BUBBLE_CHROME,
        this);  // ExtensionPopupDelegate
    popup_button_ = button;
    popup_button_->SetButtonPushed();

    return;
  }

  // Otherwise, we send the action to the extension.
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      profile_, browser_action->extension_id(), browser_);
}

gfx::Size BrowserActionsContainer::GetPreferredSize() {
  if (browser_action_views_.empty())
    return gfx::Size(0, 0);

  // We calculate the size of the view by taking the current width and
  // subtracting resize_amount_ (the latter represents how far the user is
  // resizing the view or, if animating the snapping, how far to animate it).
  // But we also clamp it to a minimum size and the maximum size, so that the
  // container can never shrink too far or take up more space than it needs. In
  // other words: ContainerMinSize() < width() - resize < ClampTo(MAX).
  int width = std::max(ContainerMinSize(),
                       container_size_.width() - resize_amount_);
  int max_width = ClampToNearestIconCount(-1, false);  // -1 gives max width.
  width = std::min(width, max_width);

  return gfx::Size(width, kButtonSize);
}

void BrowserActionsContainer::Layout() {
  if (browser_action_views_.size() == 0) {
    SetVisible(false);
    resize_gripper_->SetVisible(false);
    chevron_->SetVisible(false);
    return;
  } else {
    SetVisible(true);
    resize_gripper_->SetVisible(true);
  }

  int x = 0;
  if (resize_gripper_->IsVisible()) {
    // We'll draw the resize gripper a little wider, to add some invisible hit
    // target area - but we don't account for it anywhere.
    gfx::Size sz = resize_gripper_->GetPreferredSize();
    resize_gripper_->SetBounds(x, (height() - sz.height()) / 2 + 1,
                               sz.width() + kExtraResizeArea, sz.height());
    x += sz.width();
  }

  x += base::i18n::IsRTL() ? kHorizontalPaddingRtl : kHorizontalPadding;

  // Calculate if all icons fit without showing the chevron. We need to know
  // this beforehand, because showing the chevron will decrease the space that
  // we have to draw the visible ones (ie. if one icon is visible and another
  // doesn't have enough room).
  int last_x_of_icons = x +
      (browser_action_views_.size() * kButtonSize) +
      ((browser_action_views_.size() - 1) *
          kBrowserActionButtonPadding);

  gfx::Size sz = GetPreferredSize();
  int max_x = sz.width() - kDividerHorizontalMargin - kChevronRightMargin;

  // If they don't all fit, show the chevron (unless suppressed).
  gfx::Size chevron_size;
  if (last_x_of_icons >= max_x && !suppress_chevron_) {
    chevron_->SetVisible(true);
    chevron_size = chevron_->GetPreferredSize();
    max_x -= chevron_size.width();
    chevron_->SetBounds(width() - chevron_size.width() - kChevronRightMargin,
                        kChevronTopMargin,
                        chevron_size.width(), chevron_size.height());
  } else {
    chevron_->SetVisible(false);
  }

  // Now draw the icons for the browser actions in the available space.
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionView* view = browser_action_views_[i];
    // Add padding between buttons if multiple buttons.
    int padding = (i > 0) ? kBrowserActionButtonPadding : 0;
    if (x + kButtonSize + padding < max_x) {
      x += padding;
      view->SetBounds(x, 0, kButtonSize, height());
      view->SetVisible(true);
      x += kButtonSize;
    } else {
      view->SetVisible(false);
    }
  }
}

void BrowserActionsContainer::Paint(gfx::Canvas* canvas) {
  // The one-pixel themed vertical divider to the right of the browser actions.
  int x = base::i18n::IsRTL() ?
      kDividerHorizontalMargin : (width() - kDividerHorizontalMargin);
  DetachableToolbarView::PaintVerticalDivider(
      canvas, x, height(), kDividerVerticalPadding,
      DetachableToolbarView::kEdgeDividerColor,
      DetachableToolbarView::kMiddleDividerColor,
      GetThemeProvider()->GetColor(BrowserThemeProvider::COLOR_TOOLBAR));

  // The two-pixel width drop indicator.
  if (drop_indicator_position_ > -1) {
    x = drop_indicator_position_;
    int y = kDividerVerticalPadding;
    gfx::Rect indicator_bounds(x - kDropIndicatorWidth / 2,
                               y,
                               kDropIndicatorWidth,
                               height() - (2 * kDividerVerticalPadding));

    // TODO(sky/glen): make me pretty!
    canvas->FillRectInt(kDropIndicatorColor, indicator_bounds.x(),
                        indicator_bounds.y(), indicator_bounds.width(),
                        indicator_bounds.height());
  }
}

void BrowserActionsContainer::ViewHierarchyChanged(bool is_add,
                                                   views::View* parent,
                                                   views::View* child) {
  // No extensions (e.g., incognito).
  if (!model_)
    return;

  if (is_add && child == this) {
    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because AddBrowserAction
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    CreateBrowserActionViews();
  }
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats, std::set<OSExchangeData::CustomFormat>* custom_formats) {
  custom_formats->insert(BrowserActionDragData::GetBrowserActionCustomFormat());

  return true;
}

bool BrowserActionsContainer::AreDropTypesRequired() {
  return true;
}

bool BrowserActionsContainer::CanDrop(const OSExchangeData& data) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(data))
    return false;
  return drop_data.IsFromProfile(profile_);
}

void BrowserActionsContainer::OnDragEntered(
    const views::DropTargetEvent& event) {
}

int BrowserActionsContainer::OnDragUpdated(
    const views::DropTargetEvent& event) {
  // First check if we are above the chevron (overflow) menu.
  if (GetViewForPoint(event.location()) == chevron_) {
    if (show_menu_task_factory_.empty() && !overflow_menu_)
      StartShowFolderDropMenuTimer();
    return DragDropTypes::DRAG_MOVE;
  } else {
    StopShowFolderDropMenuTimer();
  }

  // Modifying the x value before clamping affects how far you have to drag to
  // get the drop indicator to shift to another position. Modifying after
  // clamping affects where the drop indicator is drawn.

  // We add half a button size so that when you drag a button to the right and
  // you are half-way dragging across a button the drop indicator moves from the
  // left of that button to the right of that button.
  int x = event.x() + (kButtonSize / 2) + (2 * kBrowserActionButtonPadding);
  if (chevron_->IsVisible())
    x += chevron_->bounds().width();
  x = ClampToNearestIconCount(x, false);

  if (!base::i18n::IsRTL() && chevron_->IsVisible()) {
    // The clamping function includes the chevron width. In LTR locales, the
    // chevron is on the right and we never want to account for its width. In
    // RTL it is on the left and we always want to count the width.
    x -= chevron_->width();
  }

  // Clamping gives us a value where the next button will be drawn, but we want
  // to subtract the padding (and then some) to make it appear in-between the
  // buttons.
  SetDropIndicator(x - kBrowserActionButtonPadding - (base::i18n::IsRTL() ?
      kDropIndicatorOffsetRtl : kDropIndicatorOffsetLtr));
  return DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
  StopShowFolderDropMenuTimer();
  drop_indicator_position_ = -1;
  SchedulePaint();
}

int BrowserActionsContainer::OnPerformDrop(
    const views::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.GetData()))
    return DragDropTypes::DRAG_NONE;

  // Make sure we have the same view as we started with.
  DCHECK(browser_action_views_[data.index()]->button()->extension()->id() ==
         data.id());
  DCHECK(model_);

  Extension* dragging =
      browser_action_views_[data.index()]->button()->extension();

  int target_x = drop_indicator_position_;

  size_t i = 0;
  for (; i < browser_action_views_.size(); ++i) {
    int view_x =
        browser_action_views_[i]->GetBounds(APPLY_MIRRORING_TRANSFORMATION).x();
    if (!browser_action_views_[i]->IsVisible() ||
        (base::i18n::IsRTL() ? view_x < target_x : view_x >= target_x)) {
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

  model_->MoveBrowserAction(dragging, i);

  OnDragExited();  // Perform clean up after dragging.
  return DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::GetAccessibleRole(
    AccessibilityTypes::Role* role) {
  DCHECK(role);
  *role = AccessibilityTypes::ROLE_GROUPING;
  return true;
}

void BrowserActionsContainer::MoveBrowserAction(
    const std::string& extension_id, size_t new_index) {
  ExtensionsService* service = profile_->GetExtensionsService();
  if (service) {
    Extension* extension = service->GetExtensionById(extension_id, false);
    model_->MoveBrowserAction(extension, new_index);
    SchedulePaint();
  }
}

void BrowserActionsContainer::RunMenu(View* source, const gfx::Point& pt) {
  if (source == chevron_) {
    overflow_menu_ = new BrowserActionOverflowMenuController(
        this, chevron_, browser_action_views_, VisibleBrowserActions());
    overflow_menu_->set_observer(this);
    overflow_menu_->RunMenu(GetWindow()->GetNativeWindow(), false);
  }
}

void BrowserActionsContainer::WriteDragData(View* sender,
                                            const gfx::Point& press_pt,
                                            OSExchangeData* data) {
  DCHECK(data);

  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionButton* button = browser_action_views_[i]->button();
    if (button == sender) {
      // Set the dragging image for the icon.
      scoped_ptr<gfx::Canvas> canvas(
          browser_action_views_[i]->GetIconWithBadge());
      drag_utils::SetDragImageOnDataObject(*canvas, button->size(), press_pt,
                                           data);

      // Fill in the remaining info.
      BrowserActionDragData drag_data(
          browser_action_views_[i]->button()->extension()->id(), i);
      drag_data.Write(profile_, data);
      break;
    }
  }
}

int BrowserActionsContainer::GetDragOperations(View* sender,
                                               const gfx::Point& p) {
  return DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDrag(View* sender,
                                           const gfx::Point& press_pt,
                                           const gfx::Point& p) {
  return true;
}

int BrowserActionsContainer::ClampToNearestIconCount(
    int pixelWidth, bool allow_shrink_to_minimum) const {
  // Calculate the width of one icon.
  int icon_width = (kButtonSize + kBrowserActionButtonPadding);

  // Calculate pixel count for the area not used by the icons.
  int extras = WidthOfNonIconArea();

  size_t icon_count = 0u;
  if (pixelWidth >= 0) {
    // Caller wants to know how many icons fit within a given space so we start
    // by subtracting the padding, gripper and dividers.
    int icon_area = pixelWidth - extras;
    icon_area = std::max(0, icon_area);

    // Make sure we never throw an icon into the chevron menu just because
    // there isn't enough enough space for the invisible padding around buttons.
    icon_area += kBrowserActionButtonPadding - 1;

    // Count the number of icons that fit within that area.
    icon_count = icon_area / icon_width;

    if (icon_count == 0 && allow_shrink_to_minimum) {
      extras = ContainerMinSize();  // Allow very narrow width if no icons.
    } else if (icon_count > browser_action_views_.size()) {
      // No use allowing more than what we have.
      icon_count = browser_action_views_.size();
    }
  } else {
    // A negative |pixels| count indicates caller wants to know the max width
    // that fits all icons;
    icon_count = browser_action_views_.size();
  }

  return extras + (icon_count * icon_width);
}

void BrowserActionsContainer::BrowserActionAdded(Extension* extension,
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

  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);

  // Before we change anything, determine the number of visible browser actions.
  size_t visible_actions = VisibleBrowserActions();

  // Add the new browser action to the vector and the view hierarchy.
  BrowserActionView* view = new BrowserActionView(extension, this);
  browser_action_views_.insert(browser_action_views_.begin() + index, view);
  AddChildView(index, view);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // For details on why we do the following see the class comments in the
  // header.

  // Determine if we need to increase (we only do that if the container was
  // showing all icons before the addition of this icon).
  if (model_->GetVisibleIconCount() >= 0 || extension->being_upgraded()) {
    // Some icons were hidden, don't increase the size of the container.
    OnBrowserActionVisibilityChanged();
  } else {
    // Container was at max, increase the size of it by one icon.
    int target_size = IconCountToWidth(visible_actions + 1);

    // We don't want the chevron to appear while we animate. See documentation
    // in the header for why we do this.
    suppress_chevron_ = !chevron_->IsVisible();

    Animate(Tween::LINEAR, target_size);
  }
}

void BrowserActionsContainer::BrowserActionRemoved(Extension* extension) {
  CloseOverflowMenu();

  if (popup_ && popup_->host()->extension() == extension)
    HidePopup();

  // Before we change anything, determine the number of visible browser
  // actions.
  size_t visible_actions = VisibleBrowserActions();

  for (BrowserActionViews::iterator iter =
       browser_action_views_.begin(); iter != browser_action_views_.end();
       ++iter) {
    if ((*iter)->button()->extension() == extension) {
      RemoveChildView(*iter);
      delete *iter;
      browser_action_views_.erase(iter);

      // If the extension is being upgraded we don't want the bar to shrink
      // because the icon is just going to get re-added to the same location.
      if (extension->being_upgraded())
        return;

      // For details on why we do the following see the class comments in the
      // header.

      // Calculate the target size we'll animate to (end state). This might be
      // the same size (if the icon we are removing is in the overflow bucket
      // and there are other icons there). We don't decrement visible_actions
      // because we want the container to stay the same size (clamping will take
      // care of shrinking the container if there aren't enough icons to show).
      int target_size =
          ClampToNearestIconCount(IconCountToWidth(visible_actions), true);

      Animate(Tween::EASE_OUT, target_size);
      return;
    }
  }
}

void BrowserActionsContainer::BrowserActionMoved(Extension* extension,
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

void BrowserActionsContainer::ModelLoaded() {
  SetContainerWidth();
}

void BrowserActionsContainer::SetContainerWidth() {
  int visible_actions = model_->GetVisibleIconCount();
  if (visible_actions < 0)  // All icons should be visible.
    visible_actions = model_->size();
  else
    chevron_->SetVisible(true);
  container_size_ = gfx::Size(IconCountToWidth(visible_actions), kButtonSize);
}

int BrowserActionsContainer::WidthOfNonIconArea() const {
  int chevron_size = (chevron_->IsVisible()) ?
                     chevron_->GetPreferredSize().width() : 0;
  int padding = base::i18n::IsRTL() ?
      kHorizontalPaddingRtl : kHorizontalPadding;
  return resize_gripper_->GetPreferredSize().width() + padding +
         chevron_size + kChevronRightMargin + kDividerHorizontalMargin;
}

int BrowserActionsContainer::IconCountToWidth(int icons) const {
  DCHECK_GE(icons, 0);
  if (icons == 0)
    return ContainerMinSize();

  int icon_width = kButtonSize + kBrowserActionButtonPadding;

  return WidthOfNonIconArea() + (icons * icon_width);
}

int BrowserActionsContainer::ContainerMinSize() const {
  return resize_gripper_->width() + chevron_->width() + kChevronRightMargin;
}

void BrowserActionsContainer::Animate(Tween::Type tween_type, int target_size) {
  if (!disable_animations_during_testing_) {
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

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    if (browser_action_views_[i]->IsVisible())
      ++visible_actions;
  }

  return visible_actions;
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  if (!done_resizing) {
    resize_amount_ = resize_amount;
    OnBrowserActionVisibilityChanged();
  } else {
    // For details on why we do the following see the class comments in the
    // header.

    // Clamp lower limit to 0 and upper limit to the amount that allows enough
    // room for all icons to show.
    int new_width = std::max(0, container_size_.width() - resize_amount);
    int max_width = ClampToNearestIconCount(-1, false);
    new_width = std::min(new_width, max_width);

    // Up until now we've only been modifying the resize_amount, but now it is
    // time to set the container size to the size we have resized to, but then
    // animate to the nearest icon count size (or down to min size if no icon).
    container_size_.set_width(new_width);
    animation_target_size_ = ClampToNearestIconCount(new_width, true);
    resize_animation_->Reset();
    resize_animation_->SetTweenType(Tween::EASE_OUT);
    resize_animation_->Show();
  }
}

void BrowserActionsContainer::AnimationProgressed(const Animation* animation) {
  DCHECK(animation == resize_animation_.get());

  double e = resize_animation_->GetCurrentValue();
  int difference = container_size_.width() - animation_target_size_;

  resize_amount_ = static_cast<int>(e * difference);

  OnBrowserActionVisibilityChanged();
}

void BrowserActionsContainer::AnimationEnded(const Animation* animation) {
  container_size_.set_width(animation_target_size_);
  animation_target_size_ = 0;
  resize_amount_ = 0;
  OnBrowserActionVisibilityChanged();
  suppress_chevron_ = false;

  // Don't save the icon count in incognito because there may be fewer icons
  // in that mode. The result is that the container in a normal window is always
  // at least as wide as in an incognito window.
  if (!profile_->IsOffTheRecord())
    model_->SetVisibleIconCount(VisibleBrowserActions());
}

void BrowserActionsContainer::NotifyMenuDeleted(
    BrowserActionOverflowMenuController* controller) {
  DCHECK(controller == overflow_menu_);
  overflow_menu_ = NULL;
}

void BrowserActionsContainer::InspectPopup(
    ExtensionAction* action) {
  OnBrowserActionExecuted(GetBrowserActionView(action)->button(),
      true);  // |inspect_with_devtools|.
}

void BrowserActionsContainer::ExtensionPopupIsClosing(ExtensionPopup* popup) {
  // ExtensionPopup is ref-counted, so we don't need to delete it.
  DCHECK_EQ(popup_, popup);
  popup_ = NULL;
  popup_button_->SetButtonNotPushed();
  popup_button_ = NULL;
}

bool BrowserActionsContainer::ShouldDisplayBrowserAction(Extension* extension) {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          profile_->GetExtensionsService()->IsIncognitoEnabled(extension));
}
