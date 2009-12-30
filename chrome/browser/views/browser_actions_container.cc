// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_actions_container.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/text_button.h"

// The size (both dimensions) of the buttons for page actions.
static const int kButtonSize = 29;

// The padding between the browser actions and the omnibox/page menu.
static const int kHorizontalPadding = 4;

// The padding between browser action buttons. Visually, the actual number of
// empty (non-drawing) pixels is this value + 2 when adjacent browser icons
// use their maximum allowed size.
static const int kBrowserActionButtonPadding = 3;

// This is the same value from toolbar.cc. We position the browser actions
// container flush with the edges of the toolbar as a special case so that we
// can draw the badge outside the visual bounds of the container.
static const int kControlVertOffset = 6;

// The maximum of the minimum number of browser actions present when there is
// not enough space to fit all the browser actions in the toolbar.
static const int kMinimumNumberOfVisibleBrowserActions = 2;


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
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

bool BrowserActionButton::IsPopup() {
  return browser_action_->has_popup();
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
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  ExtensionsService* extension_service = profile->GetExtensionsService();
  if (!extension_service)  // The |extension_service| can be NULL in Incognito.
    return;

  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(profile_));

  for (size_t i = 0; i < extension_service->extensions()->size(); ++i)
    AddBrowserAction(extension_service->extensions()->at(i));

  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);
}

BrowserActionsContainer::~BrowserActionsContainer() {
  HidePopup();
  DeleteBrowserActionViews();
}

int BrowserActionsContainer::GetCurrentTabId() {
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

void BrowserActionsContainer::AddBrowserAction(Extension* extension) {
#if defined(DEBUG)
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    DCHECK(browser_action_views_[i]->button()->extension() != extension) <<
           "Asked to add a browser action view for an extension that already "
           "exists.";
  }
#endif
  if (!extension->browser_action())
    return;

  BrowserActionView* view = new BrowserActionView(extension, this);
  browser_action_views_.push_back(view);
  AddChildView(view);
  if (GetParent())
    GetParent()->SchedulePaint();
}

void BrowserActionsContainer::RemoveBrowserAction(Extension* extension) {
  if (!extension->browser_action())
    return;

  if (popup_ && popup_->host()->extension() == extension) {
    HidePopup();
  }

  for (std::vector<BrowserActionView*>::iterator iter =
       browser_action_views_.begin(); iter != browser_action_views_.end();
       ++iter) {
    if ((*iter)->button()->extension() == extension) {
      RemoveChildView(*iter);
      delete *iter;
      browser_action_views_.erase(iter);
      if (GetParent())
        GetParent()->SchedulePaint();
      return;
    }
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
  toolbar_->Layout();
}

void BrowserActionsContainer::HidePopup() {
  if (popup_) {
    // This sometimes gets called via a timer (See BubbleLostFocus), so clear
    // the task factory. in case one is pending.
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

    gfx::Point origin;
    View::ConvertPointToScreen(button, &origin);
    gfx::Rect rect = button->bounds();
    rect.set_x(origin.x());
    rect.set_y(origin.y());

    gfx::NativeWindow frame_window =
        toolbar_->browser()->window()->GetNativeHandle();
    BubbleBorder::ArrowLocation arrow_location = UILayoutIsRightToLeft() ?
        BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;
    popup_ = ExtensionPopup::Show(browser_action->popup_url(),
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
  int width = kHorizontalPadding * 2 +
      browser_action_views_.size() * kButtonSize;
  if (browser_action_views_.size() > 1)
    width += (browser_action_views_.size() - 1) * kBrowserActionButtonPadding;
  return gfx::Size(width, kButtonSize);
}

void BrowserActionsContainer::Layout() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionView* view = browser_action_views_[i];
    int x = kHorizontalPadding +
        i * (kButtonSize + kBrowserActionButtonPadding);
    if (x + kButtonSize <= width()) {
      view->SetBounds(x, 0, kButtonSize, height());
      view->SetVisible(true);
    } else {
      view->SetVisible(false);
    }
  }
}

void BrowserActionsContainer::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
      AddBrowserAction(Details<Extension>(details).ptr());
      OnBrowserActionVisibilityChanged();
      break;

    case NotificationType::EXTENSION_UNLOADED:
    case NotificationType::EXTENSION_UNLOADED_DISABLED:
      RemoveBrowserAction(Details<Extension>(details).ptr());
      OnBrowserActionVisibilityChanged();
      break;

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

int BrowserActionsContainer::GetClippedPreferredWidth(int available_width) {
  if (browser_action_views_.size() == 0)
    return 0;

  // We have at least one browser action. Make some of them sticky.
  int min_width = kHorizontalPadding * 2 +
      std::min(static_cast<int>(browser_action_views_.size()),
               kMinimumNumberOfVisibleBrowserActions) * kButtonSize;

  // Even if available_width is <= 0, we still return at least the |min_width|.
  if (available_width <= 0)
    return min_width;

  return std::max(min_width, available_width - available_width % kButtonSize +
                  kHorizontalPadding * 2);
}
