// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_actions_container.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/image_loading_tracker.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/button/text_button.h"

// The size of the icon for page actions.
static const int kIconSize = 30;

// The padding between the browser actions and the omnibox/page menu.
static const int kHorizontalPadding = 4;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionImageView

// The BrowserActionImageView is a specialization of the TextButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread to
class BrowserActionImageView : public views::TextButton,
                               public views::ButtonListener,
                               public ImageLoadingTracker::Observer,
                               public NotificationObserver {
 public:
  BrowserActionImageView(ExtensionAction* browser_action,
                         Extension* extension,
                         BrowserActionsContainer* panel);
  ~BrowserActionImageView();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(SkBitmap* image, size_t index);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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

  DISALLOW_COPY_AND_ASSIGN(BrowserActionImageView);
};

BrowserActionImageView::BrowserActionImageView(
    ExtensionAction* browser_action, Extension* extension,
    BrowserActionsContainer* panel)
    : TextButton(this, L""),
      browser_action_(browser_action),
      browser_action_state_(extension->browser_action_state()),
      tracker_(NULL),
      panel_(panel) {
  set_alignment(TextButton::ALIGN_CENTER);

  // Load the images this view needs asynchronously on the file thread. We'll
  // get a call back into OnImageLoaded if the image loads successfully. If not,
  // the ImageView will have no image and will not appear in the browser chrome.
  DCHECK(!browser_action->icon_paths().empty());
  const std::vector<std::string>& icon_paths = browser_action->icon_paths();
  browser_action_icons_.resize(icon_paths.size());
  tracker_ = new ImageLoadingTracker(this, icon_paths.size());
  for (std::vector<std::string>::const_iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    FilePath path = extension->GetResourcePath(*iter);
    tracker_->PostLoadImageTask(path);
  }

  registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                 Source<ExtensionAction>(browser_action_));
}

BrowserActionImageView::~BrowserActionImageView() {
  if (tracker_) {
    tracker_->StopTrackingImageLoad();
    tracker_ = NULL;  // The tracker object will be deleted when we return.
  }
}

void BrowserActionImageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  panel_->OnBrowserActionExecuted(*browser_action_);
}

void BrowserActionImageView::OnImageLoaded(SkBitmap* image, size_t index) {
  DCHECK(index < browser_action_icons_.size());
  browser_action_icons_[index] = *image;
  if (index == browser_action_icons_.size() - 1) {
    OnStateUpdated();
    tracker_ = NULL;  // The tracker object will delete itself when we return.
  }
}

void BrowserActionImageView::OnStateUpdated() {
  SkBitmap* image = &browser_action_icons_[browser_action_state_->icon_index()];
  SetIcon(*image);
  SetTooltipText(ASCIIToWide(browser_action_state_->title()));
  panel_->OnBrowserActionVisibilityChanged();
}

void BrowserActionImageView::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED) {
    OnStateUpdated();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(
    Profile* profile, ToolbarView* toolbar)
    : profile_(profile), toolbar_(toolbar) {
  ExtensionsService* extension_service = profile->GetExtensionsService();
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<ExtensionsService>(extension_service));

  RefreshBrowserActionViews();
}

BrowserActionsContainer::~BrowserActionsContainer() {
  DeleteBrowserActionViews();
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  if (!extension_service)  // The |extension_service| can be NULL in Incognito.
    return;

  std::vector<ExtensionAction*> browser_actions;
  browser_actions = extension_service->GetBrowserActions();

  DeleteBrowserActionViews();
  for (size_t i = 0; i < browser_actions.size(); ++i) {
    Extension* extension = extension_service->GetExtensionById(
        browser_actions[i]->extension_id());
    DCHECK(extension);

    // Only show browser actions that have an icon.
    if (browser_actions[i]->icon_paths().size() > 0) {
      BrowserActionImageView* view =
          new BrowserActionImageView(browser_actions[i], extension, this);
      browser_action_views_.push_back(view);
      AddChildView(view);
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

void BrowserActionsContainer::OnBrowserActionExecuted(
    const ExtensionAction& browser_action) {
  int window_id = ExtensionTabUtil::GetWindowId(toolbar_->browser());
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      profile_, browser_action.extension_id(), window_id);
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
    views::TextButton* view = browser_action_views_[i];
    int x = kHorizontalPadding + i * kIconSize;
    view->SetBounds(x, (height() - kIconSize) / 2, kIconSize, kIconSize);
  }
}

void BrowserActionsContainer::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED ||
      type == NotificationType::EXTENSION_UNLOADED ||
      type == NotificationType::EXTENSION_UNLOADED_DISABLED) {
    RefreshBrowserActionViews();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}
