// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"

#include "base/basictypes.h"
#include "base/mac/bundle_locations.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/menu_tracked_root_view.h"
#import "chrome/browser/ui/cocoa/wrench_menu/recent_tabs_menu_model_delegate.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_observer.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/ui/zoom/zoom_event_manager.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Padding amounts on the left/right of a custom menu item (like the browser
// actions overflow container).
const int kLeftPadding = 16;
const int kRightPadding = 10;

// In *very* extreme cases, it's possible that there are so many overflowed
// actions, we won't be able to show them all. Cap the height so that the
// overflow won't make the menu larger than the height of the screen.
// Note: With this height, we can show 104 actions. Less than 0.0002% of our
// users will be affected.
const int kMaxOverflowContainerHeight = 416;
}

namespace wrench_menu_controller {
const CGFloat kWrenchBubblePointOffsetY = 6;
}

using base::UserMetricsAction;

@interface WrenchMenuController (Private)
- (void)createModel;
- (void)adjustPositioning;
- (void)performCommandDispatch:(NSNumber*)tag;
- (NSButton*)zoomDisplay;
- (void)menu:(NSMenu*)menu willHighlightItem:(NSMenuItem*)item;
- (void)removeAllItems:(NSMenu*)menu;
- (NSMenu*)recentTabsSubmenu;
- (RecentTabsSubMenuModel*)recentTabsMenuModel;
- (int)maxWidthForMenuModel:(ui::MenuModel*)model
                 modelIndex:(int)modelIndex;
@end

namespace WrenchMenuControllerInternal {

// A C++ delegate that handles the accelerators in the wrench menu.
class AcceleratorDelegate : public ui::AcceleratorProvider {
 public:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* out_accelerator) override {
    AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
    const ui::Accelerator* accelerator =
        keymap->GetAcceleratorForCommand(command_id);
    if (!accelerator)
      return false;
    *out_accelerator = *accelerator;
    return true;
  }
};

class ZoomLevelObserver {
 public:
  ZoomLevelObserver(WrenchMenuController* controller,
                    ui_zoom::ZoomEventManager* manager)
      : controller_(controller) {
    subscription_ = manager->AddZoomLevelChangedCallback(
        base::Bind(&ZoomLevelObserver::OnZoomLevelChanged,
                   base::Unretained(this)));
  }

  ~ZoomLevelObserver() {}

 private:
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change) {
    WrenchMenuModel* wrenchMenuModel = [controller_ wrenchMenuModel];
    wrenchMenuModel->UpdateZoomControls();
    const base::string16 level =
        wrenchMenuModel->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY);
    [[controller_ zoomDisplay] setTitle:SysUTF16ToNSString(level)];
  }

  scoped_ptr<content::HostZoomMap::Subscription> subscription_;

  WrenchMenuController* controller_;  // Weak; owns this.

  DISALLOW_COPY_AND_ASSIGN(ZoomLevelObserver);
};

class ToolbarActionsBarObserverHelper : public ToolbarActionsBarObserver {
 public:
  ToolbarActionsBarObserverHelper(WrenchMenuController* controller,
                                  ToolbarActionsBar* toolbar_actions_bar)
      : controller_(controller),
        scoped_observer_(this) {
    scoped_observer_.Add(toolbar_actions_bar);
  }
  ~ToolbarActionsBarObserverHelper() override {}

 private:
  // ToolbarActionsBarObserver:
  void OnToolbarActionsBarDestroyed() override {
    scoped_observer_.RemoveAll();
  }
  void OnToolbarActionsBarDidStartResize() override {
    [controller_ updateBrowserActionsSubmenu];
  }

  WrenchMenuController* controller_;
  ScopedObserver<ToolbarActionsBar, ToolbarActionsBarObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarObserverHelper);
};

}  // namespace WrenchMenuControllerInternal

@implementation WrenchMenuController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;
    acceleratorDelegate_.reset(
        new WrenchMenuControllerInternal::AcceleratorDelegate());
    [self createModel];
  }
  return self;
}

- (void)dealloc {
  [self browserWillBeDestroyed];
  [super dealloc];
}

- (void)browserWillBeDestroyed {
  // This method indicates imminent destruction. Destroy owned objects that hold
  // a weak Browser*, or pass this call onto reference counted objects.
  recentTabsMenuModelDelegate_.reset();
  [self setModel:nullptr];
  wrenchMenuModel_.reset();
  buttonViewController_.reset();

  // The observers should most likely already be destroyed (since they're reset
  // in -menuDidClose:), but sometimes shutdown can be funny, so make sure to
  // not leave any dangling observers.
  zoom_level_observer_.reset();
  toolbar_actions_bar_observer_.reset();

  [browserActionsController_ browserWillBeDestroyed];

  browser_ = nullptr;
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model {
  // Non-button item types should be built as normal items, with the exception
  // of the extensions overflow menu.
  int command_id = model->GetCommandIdAt(index);
  if (model->GetTypeAt(index) != ui::MenuModel::TYPE_BUTTON_ITEM &&
      command_id != IDC_EXTENSIONS_OVERFLOW_MENU) {
    [super addItemToMenu:menu
                 atIndex:index
               fromModel:model];
    return;
  }

  // Handle the special-cased menu items.
  base::scoped_nsobject<NSMenuItem> customItem(
      [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""]);
  MenuTrackedRootView* view = nil;
  switch (command_id) {
    case IDC_EXTENSIONS_OVERFLOW_MENU: {
      browserActionsMenuItem_ = customItem.get();
      view = [buttonViewController_ toolbarActionsOverflowItem];
      BrowserActionsContainerView* containerView =
          [buttonViewController_ overflowActionsContainerView];

      // The overflow browser actions container can't function properly without
      // a main counterpart, so if the browser window hasn't initialized, abort.
      // (This is fine because we re-populate the wrench menu each time before
      // we show it.)
      if (!browser_->window())
        break;

      BrowserActionsController* mainController =
          [[[BrowserWindowController browserWindowControllerForWindow:browser_->
              window()->GetNativeWindow()] toolbarController]
                  browserActionsController];
      toolbar_actions_bar_observer_.reset(
          new WrenchMenuControllerInternal::ToolbarActionsBarObserverHelper(
              self, [mainController toolbarActionsBar]));
      browserActionsController_.reset(
          [[BrowserActionsController alloc]
              initWithBrowser:browser_
                containerView:containerView
               mainController:mainController]);
      break;
    }
    case IDC_EDIT_MENU:
      view = [buttonViewController_ editItem];
      break;
    case IDC_ZOOM_MENU:
      view = [buttonViewController_ zoomItem];
      break;
    default:
      NOTREACHED();
      break;
  }
  DCHECK(view);
  [customItem setView:view];
  [view setMenuItem:customItem];
  [self adjustPositioning];
  [menu insertItem:customItem.get() atIndex:index];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  const BOOL enabled = [super validateUserInterfaceItem:item];

  NSMenuItem* menuItem = (id)item;
  ui::MenuModel* model =
      static_cast<ui::MenuModel*>(
          [[menuItem representedObject] pointerValue]);

  // The section headers in the recent tabs submenu should be bold and black if
  // a font list is specified for the items (bold is already applied in the
  // |MenuController| as the font list returned by |GetLabelFontListAt| is
  // bold).
  if (model && model == [self recentTabsMenuModel]) {
    if (model->GetLabelFontListAt([item tag])) {
      DCHECK([menuItem attributedTitle]);
      base::scoped_nsobject<NSMutableAttributedString> title(
          [[NSMutableAttributedString alloc]
              initWithAttributedString:[menuItem attributedTitle]]);
      [title addAttribute:NSForegroundColorAttributeName
                    value:[NSColor blackColor]
                    range:NSMakeRange(0, [title length])];
      [menuItem setAttributedTitle:title.get()];
    } else {
      // Not a section header. Add a tooltip with the title and the URL.
      std::string url;
      base::string16 title;
      if ([self recentTabsMenuModel]->GetURLAndTitleForItemAtIndex(
              [item tag], &url, &title)) {
        [menuItem setToolTip:
            cocoa_l10n_util::TooltipForURLAndTitle(
                base::SysUTF8ToNSString(url), base::SysUTF16ToNSString(title))];
       }
    }
  }

  return enabled;
}

- (NSMenu*)bookmarkSubMenu {
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_BOOKMARKS_MENU);
  return [[[self menu] itemWithTitle:title] submenu];
}

- (void)updateBookmarkSubMenu {
  NSMenu* bookmarkMenu = [self bookmarkSubMenu];
  DCHECK(bookmarkMenu);

  bookmarkMenuBridge_.reset(
      new BookmarkMenuBridge([self wrenchMenuModel]->browser()->profile(),
                             bookmarkMenu));
}

- (void)updateBrowserActionsSubmenu {
  MenuTrackedRootView* view =
      [buttonViewController_ toolbarActionsOverflowItem];
  BrowserActionsContainerView* containerView =
      [buttonViewController_ overflowActionsContainerView];

  // Find the preferred container size for the menu width.
  int menuWidth = [[self menu] size].width;
  int maxContainerWidth = menuWidth - kLeftPadding - kRightPadding;
  // Don't let the menu change sizes on us. (We lift this restriction every time
  // the menu updates, so if something changes, this won't leave us with an
  // awkward size.)
  [[self menu] setMinimumWidth:menuWidth];
  gfx::Size preferredContainerSize =
      [browserActionsController_ sizeForOverflowWidth:maxContainerWidth];

  // Set the origins and preferred size for the container.
  // View hierarchy is as follows (from parent > child):
  // |view| > |anonymous view| > containerView. We have to set the origin
  // and size of each for it display properly.
  // The parent views each have a size of the full width of the menu, so we can
  // properly position the container.
  NSSize parentSize = NSMakeSize(menuWidth,
                                 std::min(preferredContainerSize.height(),
                                          kMaxOverflowContainerHeight));
  [view setFrameSize:parentSize];
  [[containerView superview] setFrameSize:parentSize];

  // The container view gets its preferred size.
  [containerView setFrameSize:NSMakeSize(preferredContainerSize.width(),
                                         preferredContainerSize.height())];
  [browserActionsController_ update];

  [view setFrameOrigin:NSZeroPoint];
  [[containerView superview] setFrameOrigin:NSZeroPoint];
  [containerView setFrameOrigin:NSMakePoint(kLeftPadding, 0)];
}

- (void)menuWillOpen:(NSMenu*)menu {
  [super menuWillOpen:menu];

  zoom_level_observer_.reset(
      new WrenchMenuControllerInternal::ZoomLevelObserver(
          self,
          ui_zoom::ZoomEventManager::GetForBrowserContext(
              browser_->profile())));
  NSString* title = base::SysUTF16ToNSString(
      [self wrenchMenuModel]->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY));
  [[[buttonViewController_ zoomItem] viewWithTag:IDC_ZOOM_PERCENT_DISPLAY]
      setTitle:title];
  content::RecordAction(UserMetricsAction("ShowAppMenu"));

  NSImage* icon = [self wrenchMenuModel]->browser()->window()->IsFullscreen() ?
      [NSImage imageNamed:NSImageNameExitFullScreenTemplate] :
          [NSImage imageNamed:NSImageNameEnterFullScreenTemplate];
  [[buttonViewController_ zoomFullScreen] setImage:icon];
}

- (void)menuDidClose:(NSMenu*)menu {
  [super menuDidClose:menu];
  // We don't need to observe changes to zoom or toolbar size when the menu is
  // closed, since we instantiate it with the proper value and recreate the menu
  // on each show. (We do this in -menuNeedsUpdate:, which is called when the
  // menu is about to be displayed at the start of a tracking session.)
  zoom_level_observer_.reset();
  toolbar_actions_bar_observer_.reset();
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  // First empty out the menu and create a new model.
  [self removeAllItems:menu];
  [self createModel];
  [menu setMinimumWidth:0];

  // Create a new menu, which cannot be swapped because the tracking is about to
  // start, so simply copy the items.
  NSMenu* newMenu = [self menuFromModel:model_];
  NSArray* itemArray = [newMenu itemArray];
  [self removeAllItems:newMenu];
  for (NSMenuItem* item in itemArray) {
    [menu addItem:item];
  }

  [self updateRecentTabsSubmenu];
  [self updateBookmarkSubMenu];
  [self updateBrowserActionsSubmenu];
}

// Used to dispatch commands from the Wrench menu. The custom items within the
// menu cannot be hooked up directly to First Responder because the window in
// which the controls reside is not the BrowserWindowController, but a
// NSCarbonMenuWindow; this screws up the typical |-commandDispatch:| system.
- (IBAction)dispatchWrenchMenuCommand:(id)sender {
  NSInteger tag = [sender tag];
  if (sender == [buttonViewController_ zoomPlus] ||
      sender == [buttonViewController_ zoomMinus]) {
    // Do a direct dispatch rather than scheduling on the outermost run loop,
    // which would not get hit until after the menu had closed.
    [self performCommandDispatch:[NSNumber numberWithInt:tag]];

    // The zoom buttons should not close the menu if opened sticky.
    if ([sender respondsToSelector:@selector(isTracking)] &&
        [sender performSelector:@selector(isTracking)]) {
      [menu_ cancelTracking];
    }
  } else {
    // The custom views within the Wrench menu are abnormal and keep the menu
    // open after a target-action.  Close the menu manually.
    [menu_ cancelTracking];

    // Executing certain commands from the nested run loop of the menu can lead
    // to wonky behavior (e.g. http://crbug.com/49716). To avoid this, schedule
    // the dispatch on the outermost run loop.
    [self performSelector:@selector(performCommandDispatch:)
               withObject:[NSNumber numberWithInt:tag]
               afterDelay:0.0];
  }
}

// Used to perform the actual dispatch on the outermost runloop.
- (void)performCommandDispatch:(NSNumber*)tag {
  [self wrenchMenuModel]->ExecuteCommand([tag intValue], 0);
}

- (WrenchMenuModel*)wrenchMenuModel {
  // Don't use |wrenchMenuModel_| so that a test can override the generic one.
  return static_cast<WrenchMenuModel*>(model_);
}

- (void)updateRecentTabsSubmenu {
  ui::MenuModel* model = [self recentTabsMenuModel];
  if (model) {
    recentTabsMenuModelDelegate_.reset(
        new RecentTabsMenuModelDelegate(model, [self recentTabsSubmenu]));
  }
}

- (BrowserActionsController*)browserActionsController {
  return browserActionsController_.get();
}

- (void)createModel {
  DCHECK(browser_);
  recentTabsMenuModelDelegate_.reset();
  wrenchMenuModel_.reset(
      new WrenchMenuModel(acceleratorDelegate_.get(), browser_));
  [self setModel:wrenchMenuModel_.get()];

  buttonViewController_.reset(
      [[WrenchMenuButtonViewController alloc] initWithController:self]);
  [buttonViewController_ view];

  // See comment in containerSuperviewFrameChanged:.
  NSView* containerSuperview =
      [[buttonViewController_ overflowActionsContainerView] superview];
  [containerSuperview setPostsFrameChangedNotifications:YES];
}

// Fit the localized strings into the Cut/Copy/Paste control, then resize the
// whole menu item accordingly.
- (void)adjustPositioning {
  const CGFloat kButtonPadding = 12;
  CGFloat delta = 0;

  // Go through the three buttons from right-to-left, adjusting the size to fit
  // the localized strings while keeping them all aligned on their horizontal
  // edges.
  NSButton* views[] = {
      [buttonViewController_ editPaste],
      [buttonViewController_ editCopy],
      [buttonViewController_ editCut]
  };
  for (size_t i = 0; i < arraysize(views); ++i) {
    NSButton* button = views[i];
    CGFloat originalWidth = NSWidth([button frame]);

    // Do not let |-sizeToFit| change the height of the button.
    NSSize size = [button frame].size;
    [button sizeToFit];
    size.width = [button frame].size.width + kButtonPadding;
    [button setFrameSize:size];

    CGFloat newWidth = size.width;
    delta += newWidth - originalWidth;

    NSRect frame = [button frame];
    frame.origin.x -= delta;
    [button setFrame:frame];
  }

  // Resize the menu item by the total amound the buttons changed so that the
  // spacing between the buttons and the title remains the same.
  NSRect itemFrame = [[buttonViewController_ editItem] frame];
  itemFrame.size.width += delta;
  [[buttonViewController_ editItem] setFrame:itemFrame];

  // Also resize the superview of the buttons, which is an NSView used to slide
  // when the item title is too big and GTM resizes it.
  NSRect parentFrame = [[[buttonViewController_ editCut] superview] frame];
  parentFrame.size.width += delta;
  parentFrame.origin.x -= delta;
  [[[buttonViewController_ editCut] superview] setFrame:parentFrame];
}

- (NSButton*)zoomDisplay {
  return [buttonViewController_ zoomDisplay];
}

- (void)menu:(NSMenu*)menu willHighlightItem:(NSMenuItem*)item {
  if (browserActionsController_.get()) {
    [browserActionsController_ setFocusedInOverflow:
        (item == browserActionsMenuItem_)];
  }
}

// -[NSMenu removeAllItems] is only available on 10.6+.
- (void)removeAllItems:(NSMenu*)menu {
  while ([menu numberOfItems]) {
    [menu removeItemAtIndex:0];
  }
}

- (NSMenu*)recentTabsSubmenu {
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_RECENT_TABS_MENU);
  return [[[self menu] itemWithTitle:title] submenu];
}

// The recent tabs menu model is recognized by the existence of either the
// kRecentlyClosedHeaderCommandId or the kDisabledRecentlyClosedHeaderCommandId.
- (RecentTabsSubMenuModel*)recentTabsMenuModel {
  int index = 0;
  // Start searching at the wrench menu model level, |model| will be updated
  // only if the command we're looking for is found in one of the [sub]menus.
  ui::MenuModel* model = [self wrenchMenuModel];
  if (ui::MenuModel::GetModelAndIndexForCommandId(
          RecentTabsSubMenuModel::kRecentlyClosedHeaderCommandId, &model,
          &index)) {
    return static_cast<RecentTabsSubMenuModel*>(model);
  }
  if (ui::MenuModel::GetModelAndIndexForCommandId(
          RecentTabsSubMenuModel::kDisabledRecentlyClosedHeaderCommandId,
          &model, &index)) {
    return static_cast<RecentTabsSubMenuModel*>(model);
  }
  return NULL;
}

// This overrdies the parent class to return a custom width for recent tabs
// menu.
- (int)maxWidthForMenuModel:(ui::MenuModel*)model
                 modelIndex:(int)modelIndex {
  RecentTabsSubMenuModel* recentTabsMenuModel = [self recentTabsMenuModel];
  if (recentTabsMenuModel && recentTabsMenuModel == model) {
    return recentTabsMenuModel->GetMaxWidthForItemAtIndex(modelIndex);
  }
  return -1;
}

@end  // @implementation WrenchMenuController

////////////////////////////////////////////////////////////////////////////////

@interface WrenchMenuButtonViewController ()
- (void)containerSuperviewFrameChanged:(NSNotification*)notification;
@end

@implementation WrenchMenuButtonViewController

@synthesize editItem = editItem_;
@synthesize editCut = editCut_;
@synthesize editCopy = editCopy_;
@synthesize editPaste = editPaste_;
@synthesize zoomItem = zoomItem_;
@synthesize zoomPlus = zoomPlus_;
@synthesize zoomDisplay = zoomDisplay_;
@synthesize zoomMinus = zoomMinus_;
@synthesize zoomFullScreen = zoomFullScreen_;
@synthesize toolbarActionsOverflowItem = toolbarActionsOverflowItem_;
@synthesize overflowActionsContainerView = overflowActionsContainerView_;

- (id)initWithController:(WrenchMenuController*)controller {
  if ((self = [super initWithNibName:@"WrenchMenu"
                              bundle:base::mac::FrameworkBundle()])) {
    controller_ = controller;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerSuperviewFrameChanged:)
               name:NSViewFrameDidChangeNotification
             object:[overflowActionsContainerView_ superview]];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (IBAction)dispatchWrenchMenuCommand:(id)sender {
  [controller_ dispatchWrenchMenuCommand:sender];
}

- (void)containerSuperviewFrameChanged:(NSNotification*)notification {
  // AppKit menus were probably never designed with a view like the browser
  // actions container in mind, and, as a result, we come across a few oddities.
  // One of these is that the container's superview will, on some versions of
  // OSX, change frame position sometime after the the menu begins tracking
  // (and thus, after all our ability to adjust it normally). Throw in the
  // towel, and simply don't let the frame move from where it's supposed to be.
  // TODO(devlin): Yet another Cocoa hack. It'd be good to find a workaround,
  // but unlikely unless we replace the Cocoa menu implementation.
  NSView* containerSuperview = [overflowActionsContainerView_ superview];
  if (NSMinX([containerSuperview frame]) != 0)
    [containerSuperview setFrameOrigin:NSZeroPoint];
}

@end  // @implementation WrenchMenuButtonViewController
