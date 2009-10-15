// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_actions_container.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"

// The size of the icon for page actions.
static const int kIconSize = 29;

// The padding between the browser actions and the omnibox/page menu.
static const int kHorizontalPadding = 4;

// This is the same value from toolbar.cc. We position the browser actions
// container flush with the edges of the toolbar as a special case so that we
// can draw the badge outside the visual bounds of the container.
static const int kControlVertOffset = 6;

// The maximum of the minimum number of browser actions present when there is
// not enough space to fit all the browser actions in the toolbar.
static const int kMinimumNumberOfVisibleBrowserActions = 2;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread to
class BrowserActionButton : public views::MenuButton,
                            public views::ButtonListener,
                            public ImageLoadingTracker::Observer,
                            public NotificationObserver {
 public:
  BrowserActionButton(ExtensionAction* browser_action,
                      Extension* extension,
                      BrowserActionsContainer* panel);
  ~BrowserActionButton();

  const ExtensionAction& browser_action() const { return *browser_action_; }
  ExtensionActionState* browser_action_state() { return browser_action_state_; }

  // Overriden from views::View. Return a 0-inset so the icon can draw all the
  // way to the edge of the view if it wants.
  virtual gfx::Insets GetInsets() const;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(SkBitmap* image, size_t index);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // MenuButton behavior overrides.  These methods all default to TextButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  virtual bool Activate();
  virtual bool OnMousePressed(const views::MouseEvent& e);
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual void OnMouseExited(const views::MouseEvent& event);

  // Does this button's action have a popup?
  virtual bool IsPopup();

  // Notifications when the popup is hidden or shown by the container.
  virtual void PopupDidShow();
  virtual void PopupDidHide();

 private:
  // Called to update the display to match the browser action's state.
  void OnStateUpdated();

  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* browser_action_;

  // The state of our browser action. Not owned by this class.
  ExtensionActionState* browser_action_state_;

  // The icons representing different states for the browser action.
  std::vector<SkBitmap> browser_action_icons_;

  // The object that is waiting for the image loading to complete
  // asynchronously. This object can potentially outlive the BrowserActionView,
  // and takes care of deleting itself.
  ImageLoadingTracker* tracker_;

  // The browser action shelf.
  BrowserActionsContainer* panel_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};

BrowserActionButton::BrowserActionButton(
    ExtensionAction* browser_action, Extension* extension,
    BrowserActionsContainer* panel)
    : MenuButton(this, L"", NULL, false),
      browser_action_(browser_action),
      browser_action_state_(extension->browser_action_state()),
      tracker_(NULL),
      panel_(panel) {
  set_alignment(TextButton::ALIGN_CENTER);

  // Load the images this view needs asynchronously on the file thread. We'll
  // get a call back into OnImageLoaded if the image loads successfully. If not,
  // the ImageView will have no image and will not appear in the browser chrome.
  if (!browser_action->icon_paths().empty()) {
    const std::vector<std::string>& icon_paths = browser_action->icon_paths();
    browser_action_icons_.resize(icon_paths.size());
    tracker_ = new ImageLoadingTracker(this, icon_paths.size());
    for (std::vector<std::string>::const_iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      tracker_->PostLoadImageTask(extension->GetResource(*iter));
    }
  }

  registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                 Source<ExtensionAction>(browser_action_));
}

BrowserActionButton::~BrowserActionButton() {
  if (tracker_) {
    tracker_->StopTrackingImageLoad();
    tracker_ = NULL;  // The tracker object will be deleted when we return.
  }
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
  DCHECK(index < browser_action_icons_.size());
  browser_action_icons_[index] = image ? *image : SkBitmap();
  if (index == browser_action_icons_.size() - 1) {
    OnStateUpdated();
    tracker_ = NULL;  // The tracker object will delete itself when we return.
  }
}

void BrowserActionButton::OnStateUpdated() {
  SkBitmap* image = browser_action_state_->icon();
  if (!image) {
    if (static_cast<size_t>(browser_action_state_->icon_index()) <
        browser_action_icons_.size()) {
      image = &browser_action_icons_[browser_action_state_->icon_index()];
    }
  }

  if (image)
    SetIcon(*image);

  SetTooltipText(ASCIIToWide(browser_action_state_->title()));
  panel_->OnBrowserActionVisibilityChanged();
  GetParent()->SchedulePaint();
}

void BrowserActionButton::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED) {
    OnStateUpdated();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

bool BrowserActionButton::IsPopup() {
  return browser_action_->is_popup();
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
  if (IsPopup())
    return MenuButton::OnMousePressed(e);
  return TextButton::OnMousePressed(e);
}

void BrowserActionButton::OnMouseReleased(const views::MouseEvent& e,
                                          bool canceled) {
  if (IsPopup()) {
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
  if (IsPopup())
    MenuButton::OnMouseExited(e);
  else
    TextButton::OnMouseExited(e);
}

void BrowserActionButton::PopupDidShow() {
  SetState(views::CustomButton::BS_PUSHED);
  menu_visible_ = true;
}

void BrowserActionButton::PopupDidHide() {
  SetState(views::CustomButton::BS_NORMAL);
  menu_visible_ = false;
}


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView
// A single section in the browser action container. This contains the actual
// BrowserActionButton, as well as the logic to paint the badge.

class BrowserActionView : public views::View {
 public:
  BrowserActionView(ExtensionAction* browser_action, Extension* extension,
                    BrowserActionsContainer* panel);

 private:
  virtual void Layout();

  // Override PaintChildren so that we can paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas);

  // The button this view contains.
  BrowserActionButton* button_;
};

BrowserActionView::BrowserActionView(ExtensionAction* browser_action,
                                     Extension* extension,
                                     BrowserActionsContainer* panel) {
  button_ = new BrowserActionButton(browser_action, extension, panel);
  AddChildView(button_);
}

void BrowserActionView::Layout() {
  button_->SetBounds(0, kControlVertOffset, width(),
                     height() - 2 * kControlVertOffset);
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);

  const std::string& text = button_->browser_action_state()->badge_text();
  if (text.empty())
    return;

  const int kTextSize = 8;
  const int kBottomMargin = 5;
  const int kPadding = 2;
  const int kBadgeHeight = 11;
  const int kMaxTextWidth = 23;
  const int kCenterAlignThreshold = 20;  // at than width, we center align

  canvas->save();

  SkTypeface* typeface = SkTypeface::CreateFromName("Arial", SkTypeface::kBold);
  SkPaint text_paint;
  text_paint.setAntiAlias(true);
  text_paint.setColor(SK_ColorWHITE);
  text_paint.setFakeBoldText(true);
  text_paint.setTextAlign(SkPaint::kLeft_Align);
  text_paint.setTextSize(SkIntToScalar(kTextSize));
  text_paint.setTypeface(typeface);

  // Calculate text width. We clamp it to a max size.
  SkScalar text_width = text_paint.measureText(text.c_str(), text.size());
  text_width = SkIntToScalar(
      std::min(kMaxTextWidth, SkScalarFloor(text_width)));

  // Cacluate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = SkScalarFloor(text_width) + kPadding * 2;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  SkRect rect;
  rect.fBottom = SkIntToScalar(height() - kBottomMargin);
  rect.fTop = rect.fBottom - SkIntToScalar(kBadgeHeight);
  if (badge_width >= kCenterAlignThreshold) {
    rect.fLeft = SkIntToScalar((width() - badge_width) / 2);
    rect.fRight = rect.fLeft + SkIntToScalar(badge_width);
  } else {
    rect.fRight = SkIntToScalar(width());
    rect.fLeft = rect.fRight - badge_width;
  }

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(
      button_->browser_action_state()->badge_background_color());
  canvas->drawRoundRect(rect, SkIntToScalar(2), SkIntToScalar(2), rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  SkBitmap* gradient_left = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_LEFT);
  SkBitmap* gradient_right = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_RIGHT);
  SkBitmap* gradient_center = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->drawBitmap(*gradient_left, rect.fLeft, rect.fTop);
  canvas->TileImageInt(*gradient_center,
      SkScalarFloor(rect.fLeft) + gradient_left->width(),
      SkScalarFloor(rect.fTop),
      SkScalarFloor(rect.width()) - gradient_left->width() -
                    gradient_right->width(),
      SkScalarFloor(rect.height()));
  canvas->drawBitmap(*gradient_right,
      rect.fRight - SkIntToScalar(gradient_right->width()), rect.fTop);

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.fLeft += kPadding;
  rect.fRight -= kPadding;
  canvas->clipRect(rect);
  canvas->drawText(text.c_str(), text.size(),
                   rect.fLeft + (rect.width() - text_width) / 2,
                   rect.fTop + kTextSize + 1,
                   text_paint);
  canvas->restore();
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
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<ExtensionsService>(extension_service));

  RefreshBrowserActionViews();
  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);
}

BrowserActionsContainer::~BrowserActionsContainer() {
  HidePopup();
  DeleteBrowserActionViews();
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  if (!extension_service)  // The |extension_service| can be NULL in Incognito.
    return;

  // Get all browser actions, including those with popups.
  std::vector<ExtensionAction*> browser_actions;
  browser_actions = extension_service->GetBrowserActions(true);

  DeleteBrowserActionViews();
  for (size_t i = 0; i < browser_actions.size(); ++i) {
    Extension* extension = extension_service->GetExtensionById(
        browser_actions[i]->extension_id());
    DCHECK(extension);

    BrowserActionView* view =
        new BrowserActionView(browser_actions[i], extension, this);
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
  toolbar_->Layout();
}

void BrowserActionsContainer::HidePopup() {
  if (popup_) {
    popup_->DetachFromBrowser();
    delete popup_;
    popup_ = NULL;
    popup_button_->PopupDidHide();
    popup_button_ = NULL;
    return;
  }
}

void BrowserActionsContainer::OnBrowserActionExecuted(
    BrowserActionButton* button) {
  const ExtensionAction& browser_action = button->browser_action();

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
    popup_ = ExtensionPopup::Show(browser_action.popup_url(),
                                  toolbar_->browser(),
                                  rect,
                                  browser_action.popup_height());
    popup_->set_delegate(this);
    popup_button_ = button;
    popup_button_->PopupDidShow();
    return;
  }

  // Otherwise, we send the action to the extension.
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      profile_, browser_action.extension_id(), toolbar_->browser());
}

gfx::Size BrowserActionsContainer::GetPreferredSize() {
  if (browser_action_views_.empty())
    return gfx::Size(0, 0);
  int width = kHorizontalPadding * 2 +
      browser_action_views_.size() * kIconSize;
  return gfx::Size(width, kIconSize);
}

void BrowserActionsContainer::Layout() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionView* view = browser_action_views_[i];
    int x = kHorizontalPadding + i * kIconSize;
    if (x + kIconSize <= width()) {
      view->SetBounds(x, 0, kIconSize, height());
      view->SetVisible(true);
    } else {
      view->SetVisible(false);
    }
  }
}

void BrowserActionsContainer::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED ||
      type == NotificationType::EXTENSION_UNLOADED ||
      type == NotificationType::EXTENSION_UNLOADED_DISABLED) {
    RefreshBrowserActionViews();

    // All these actions may change visibility of BrowserActions.
    OnBrowserActionVisibilityChanged();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void BrowserActionsContainer::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
}

void BrowserActionsContainer::BubbleBrowserWindowClosing(
    BrowserBubble* bubble) {
  HidePopup();
}

void BrowserActionsContainer::BubbleGotFocus(BrowserBubble* bubble) {
}

void BrowserActionsContainer::BubbleLostFocus(BrowserBubble* bubble) {
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
               kMinimumNumberOfVisibleBrowserActions) * kIconSize;

  // Even if available_width is <= 0, we still return at least the |min_width|.
  if (available_width <= 0)
    return min_width;

  return std::max(min_width, available_width - available_width % kIconSize +
                  kHorizontalPadding * 2);
}
