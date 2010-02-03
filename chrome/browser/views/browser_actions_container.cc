// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_actions_container.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/detachable_toolbar_view.h"
#include "chrome/browser/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/views/extensions/browser_action_overflow_menu_controller.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/window/window.h"

#include "grit/theme_resources.h"

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

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(Extension* extension,
                                         BrowserActionsContainer* panel)
    : MenuButton(this, L"", NULL, false),
      browser_action_(extension->browser_action()),
      extension_(extension),
      tracker_(NULL),
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
  // ::OnImageLoaded() before our creator appends up to the view heirarchy, we
  // will crash. But since we know that ImageLoadingTracker is asynchronous,
  // this should be OK. And doing this in the constructor means that we don't
  // have to protect against it getting done multiple times.
  tracker_ = new ImageLoadingTracker(this, 1);
  tracker_->PostLoadImageTask(
      extension->GetResource(relative_path),
      gfx::Size(Extension::kBrowserActionIconMaxSize,
                Extension::kBrowserActionIconMaxSize));
}

BrowserActionButton::~BrowserActionButton() {
  if (tracker_)
    tracker_->StopTrackingImageLoad();
}

gfx::Insets BrowserActionButton::GetInsets() const {
  static gfx::Insets zero_inset;
  return zero_inset;
}

void BrowserActionButton::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  panel_->OnBrowserActionExecuted(this);
}

void BrowserActionButton::OnImageLoaded(SkBitmap* image, size_t index) {
  if (image)
    default_icon_ = *image;

  tracker_ = NULL;  // The tracker object will delete itself when we return.

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

  SetTooltipText(UTF8ToWide(browser_action()->GetTitle(tab_id)));
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
    panel_->OnBrowserActionExecuted(this);

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
  showing_context_menu_ = e.IsRightMouseButton();
  if (showing_context_menu_) {
    SetButtonPushed();

    // Get the top left point of this button in screen coordinates.
    gfx::Point point = gfx::Point(0, 0);
    ConvertPointToScreen(this, &point);

    // Make the menu appear below the button.
    point.Offset(0, height());

    if (!context_menu_.get())
      context_menu_.reset(new ExtensionActionContextMenu());
    context_menu_->Run(extension(), point);

    SetButtonNotPushed();
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

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::BS_PUSHED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::BS_NORMAL);
  menu_visible_ = false;
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
    Profile* profile, ToolbarView* toolbar)
    : profile_(profile),
      toolbar_(toolbar),
      popup_(NULL),
      popup_button_(NULL),
      model_(NULL),
      resize_gripper_(NULL),
      chevron_(NULL),
      suppress_chevron_(false),
      resize_amount_(0),
      animation_target_size_(0),
      drop_indicator_position_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  ExtensionsService* extension_service = profile->GetExtensionsService();
  if (!extension_service)  // The |extension_service| can be NULL in Incognito.
    return;

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(profile_));

  model_ = extension_service->toolbar_model();
  model_->AddObserver(this);

  resize_animation_.reset(new SlideAnimation(this));
  resize_gripper_ = new views::ResizeGripper(this);
  resize_gripper_->SetVisible(false);
  AddChildView(resize_gripper_);

  // TODO(glen): Come up with a new bitmap for the chevron.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* chevron_image = rb.GetBitmapNamed(IDR_BOOKMARK_BAR_CHEVRONS);
  chevron_ = new views::MenuButton(NULL, std::wstring(), this, false);
  chevron_->SetVisible(false);
  chevron_->SetIcon(*chevron_image);
  // Chevron contains >> that should point left in LTR locales.
  chevron_->EnableCanvasFlippingForRTLUI(true);
  AddChildView(chevron_);

  int predefined_width =
      profile_->GetPrefs()->GetInteger(prefs::kBrowserActionContainerWidth);
  if (predefined_width == 0) {
    // The width will never be 0 (due to container min size restriction)
    // except when no width has been saved. So, in that case ask the model
    // how many icons we'll show and set initial size to that.
    predefined_width = IconCountToWidth(model_->size());
  }
  container_size_ = gfx::Size(predefined_width, kButtonSize);
}

BrowserActionsContainer::~BrowserActionsContainer() {
  if (model_)
    model_->RemoveObserver(this);
  CloseOverflowMenu();
  HidePopup();
  DeleteBrowserActionViews();
}

// Static.
void BrowserActionsContainer::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kBrowserActionContainerWidth, 0);
}

int BrowserActionsContainer::GetCurrentTabId() const {
  TabContents* tab_contents = toolbar_->browser()->GetSelectedTabContents();
  if (!tab_contents)
    return -1;

  return tab_contents->controller().session_id().id();
}

BrowserActionView* BrowserActionsContainer::GetBrowserActionView(
    Extension* extension) {
  for (std::vector<BrowserActionView*>::iterator iter =
       browser_action_views_.begin(); iter != browser_action_views_.end();
       ++iter) {
    if ((*iter)->button()->extension() == extension)
      return *iter;
  }

  return NULL;
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i)
    browser_action_views_[i]->button()->UpdateState();
}

void BrowserActionsContainer::CloseOverflowMenu() {
  // Close the overflow menu if open (and the context menu off of that).
  if (overflow_menu_.get())
    overflow_menu_->CancelMenu();
}

void BrowserActionsContainer::CreateBrowserActionViews() {
  DCHECK(browser_action_views_.empty());
  for (ExtensionList::iterator iter = model_->begin();
       iter != model_->end(); ++iter) {
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
  resize_gripper_->SetVisible(browser_action_views_.size() > 0);

  toolbar_->Layout();
  toolbar_->SchedulePaint();
}

void BrowserActionsContainer::HidePopup() {
  if (popup_) {
    // This sometimes gets called via a timer (See BubbleLostFocus), so clear
    // the task factory in case one is pending.
    task_factory_.RevokeAll();

    // Save these variables in local temporaries since destroying the popup
    // calls BubbleLostFocus to be called, which will try to call HidePopup()
    // again if popup_ is non-null.
    ExtensionPopup* closing_popup = popup_;
    BrowserActionButton* closing_button = popup_button_;
    popup_ = NULL;
    popup_button_ = NULL;

    closing_popup->DetachFromBrowser();
    delete closing_popup;
    closing_button->SetButtonNotPushed();
    return;
  }
}

void BrowserActionsContainer::TestExecuteBrowserAction(int index) {
  BrowserActionButton* button = browser_action_views_[index]->button();
  OnBrowserActionExecuted(button);
}

void BrowserActionsContainer::OnBrowserActionExecuted(
    BrowserActionButton* button) {
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
        toolbar_->browser()->window()->GetNativeHandle();
    BubbleBorder::ArrowLocation arrow_location = UILayoutIsRightToLeft() ?
        BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;

    popup_ = ExtensionPopup::Show(button->GetPopupUrl(),
                                  toolbar_->browser(),
                                  toolbar_->browser()->profile(),
                                  frame_window,
                                  rect,
                                  arrow_location,
                                  true);  // Activate the popup window.
    popup_->set_delegate(this);
    popup_button_ = button;
    popup_button_->SetButtonPushed();
    return;
  }

  // Otherwise, we send the action to the extension.
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      profile_, browser_action->extension_id(), toolbar_->browser());
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
  if (!resize_gripper_ || !chevron_)
    return;  // These classes are not created in Incognito mode.
  if (browser_action_views_.size() == 0) {
    resize_gripper_->SetVisible(false);
    chevron_->SetVisible(false);
    return;
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

  x += UILayoutIsRightToLeft() ? kHorizontalPaddingRtl : kHorizontalPadding;

  // Calculate if all icons fit without showing the chevron. We need to know
  // this beforehand, because showing the chevron will decrease the space that
  // we have to draw the visible ones (ie. if one icon is visible and another
  // doesn't have enough room).
  int last_x_of_icons = x +
      (browser_action_views_.size() * kButtonSize) +
      ((browser_action_views_.size() - 1) *
          kBrowserActionButtonPadding);

  int max_x = width() - kDividerHorizontalMargin - kChevronRightMargin;

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
  int x = UILayoutIsRightToLeft() ? kDividerHorizontalMargin :
                                    width() - kDividerHorizontalMargin;
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

  if (!UILayoutIsRightToLeft() && chevron_->IsVisible()) {
    // The clamping function includes the chevron width. In LTR locales, the
    // chevron is on the right and we never want to account for its width. In
    // RTL it is on the left and we always want to count the width.
    x -= chevron_->width();
  }

  // Clamping gives us a value where the next button will be drawn, but we want
  // to subtract the padding (and then some) to make it appear in-between the
  // buttons.
  drop_indicator_position_ = x - kBrowserActionButtonPadding -
      (UILayoutIsRightToLeft() ? kDropIndicatorOffsetRtl :
                                 kDropIndicatorOffsetLtr);

  SchedulePaint();
  return DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
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

  Extension* dragging =
      browser_action_views_[data.index()]->button()->extension();

  int target_x = drop_indicator_position_;

  size_t i = 0;
  for (; i < browser_action_views_.size(); ++i) {
    int view_x =
        browser_action_views_[i]->GetBounds(APPLY_MIRRORING_TRANSFORMATION).x();
    if (!browser_action_views_[i]->IsVisible() ||
        (UILayoutIsRightToLeft() ? view_x < target_x : view_x >= target_x)) {
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

  model_->MoveBrowserAction(dragging, i);

  OnDragExited();  // Perform clean up after dragging.
  return DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (!popup_ || Details<ExtensionHost>(popup_->host()) != details)
        return;

      HidePopup();
      break;

    default:
      NOTREACHED() << "Unexpected notification";
  }
}

void BrowserActionsContainer::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
}

void BrowserActionsContainer::BubbleBrowserWindowClosing(
    BrowserBubble* bubble) {
  HidePopup();
}

void BrowserActionsContainer::BubbleGotFocus(BrowserBubble* bubble) {
  if (!popup_)
    return;

  // Forward the focus to the renderer.
  popup_->host()->render_view_host()->view()->Focus();
}

void BrowserActionsContainer::BubbleLostFocus(BrowserBubble* bubble,
                                              gfx::NativeView focused_view) {
  if (!popup_)
    return;

#if defined(OS_WIN)
  // Don't hide when we are loosing focus to a child window.  This is the case
  // with select popups.
  // TODO(jcampan): http://crbugs.com/29131 make that work on toolkit views
  //                so this #if defined can be removed.
  gfx::NativeView popup_native_view = popup_->native_view();
  gfx::NativeView parent = focused_view;
  while (parent = ::GetParent(parent)) {
    if (parent == popup_native_view)
      return;
  }
#endif

  // This is a bit annoying.  If you click on the button that generated the
  // current popup, then we first get this lost focus message, and then
  // we get the click action.  This results in the popup being immediately
  // shown again.  To workaround this, we put in a delay.
  MessageLoop::current()->PostTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&BrowserActionsContainer::HidePopup));
}

void BrowserActionsContainer::RunMenu(View* source, const gfx::Point& pt) {
  if (source == chevron_) {
    overflow_menu_.reset(new BrowserActionOverflowMenuController(
        this, chevron_, browser_action_views_, VisibleBrowserActions()));
    overflow_menu_->RunMenu(GetWindow()->GetNativeWindow());
  }
}

void BrowserActionsContainer::WriteDragData(
    View* sender, int press_x, int press_y, OSExchangeData* data) {
  DCHECK(data);

  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    if (browser_action_views_[i]->button() == sender) {
      BrowserActionDragData drag_data(
          browser_action_views_[i]->button()->extension()->id(), i);
      drag_data.Write(profile_, data);
      break;
    }
  }
}

int BrowserActionsContainer::GetDragOperations(View* sender, int x, int y) {
  return DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDrag(
    View* sender, int press_x, int press_y, int x, int y) {
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

  // Before we change anything, determine the number of visible browser actions.
  size_t visible_actions = VisibleBrowserActions();

  // Add the new browser action to the vector and the view hierarchy.
  BrowserActionView* view = new BrowserActionView(extension, this);
  browser_action_views_.push_back(view);
  AddChildView(index, view);

  // For details on why we do the following see the class comments in the
  // header.

  // Determine if we need to increase (we only do that if the container was
  // showing all icons before the addition of this icon). We use -1 because
  // we don't want to count the view that we just added.
  if (visible_actions < browser_action_views_.size() - 1) {
    // Some icons were hidden, don't increase the size of the container.
    OnBrowserActionVisibilityChanged();
  } else {
    // Container was at max, increase the size of it by one icon.
    animation_target_size_ = IconCountToWidth(visible_actions + 1);

    // We don't want the chevron to appear while we animate. See documentation
    // in the header for why we do this.
    suppress_chevron_ = !chevron_->IsVisible();

    // Animate!
    resize_animation_->Reset();
    resize_animation_->SetTweenType(SlideAnimation::NONE);
    resize_animation_->Show();
  }
}

void BrowserActionsContainer::BrowserActionRemoved(Extension* extension) {
  CloseOverflowMenu();

  if (popup_ && popup_->host()->extension() == extension)
    HidePopup();

  // Before we change anything, determine the number of visible browser actions.
  size_t visible_actions = VisibleBrowserActions();

  for (std::vector<BrowserActionView*>::iterator iter =
       browser_action_views_.begin(); iter != browser_action_views_.end();
       ++iter) {
    if ((*iter)->button()->extension() == extension) {
      RemoveChildView(*iter);
      delete *iter;
      browser_action_views_.erase(iter);

      // For details on why we do the following see the class comments in the
      // header.

      // Calculate the target size we'll animate to (end state). This might be
      // the same size (if the icon we are removing is in the overflow bucket
      // and there are other icons there). We don't decrement visible_actions
      // because we want the container to stay the same size (clamping will take
      // care of shrinking the container if there aren't enough icons to show).
      animation_target_size_ =
          ClampToNearestIconCount(IconCountToWidth(visible_actions), true);

      // Animate!
      resize_animation_->Reset();
      resize_animation_->SetTweenType(SlideAnimation::EASE_OUT);
      resize_animation_->Show();
      return;
    }
  }
}

void BrowserActionsContainer::BrowserActionMoved(Extension* extension,
                                                 int index) {
  DCHECK(index >= 0 && index < static_cast<int>(browser_action_views_.size()));

  DeleteBrowserActionViews();
  CreateBrowserActionViews();
  Layout();
}

int BrowserActionsContainer::WidthOfNonIconArea() const {
  int chevron_size = (chevron_->IsVisible()) ?
                     chevron_->GetPreferredSize().width() : 0;
  int padding = UILayoutIsRightToLeft() ? kHorizontalPaddingRtl :
                                          kHorizontalPadding;
  return resize_gripper_->GetPreferredSize().width() + padding +
         chevron_size + kChevronRightMargin + kDividerHorizontalMargin;
}

int BrowserActionsContainer::IconCountToWidth(int icons) const {
  DCHECK(icons >= 0);
  if (icons == 0)
    return ContainerMinSize();

  int icon_width = kButtonSize + kBrowserActionButtonPadding;

  return WidthOfNonIconArea() + (icons * icon_width);
}

int BrowserActionsContainer::ContainerMinSize() const {
  return resize_gripper_->width() + chevron_->width() + kChevronRightMargin;
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
    resize_animation_->SetTweenType(SlideAnimation::EASE_OUT);
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

  profile_->GetPrefs()->SetInteger(prefs::kBrowserActionContainerWidth,
                                   container_size_.width());
}
