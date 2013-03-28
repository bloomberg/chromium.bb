// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"

#include "base/basictypes.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/menu_tracked_root_view.h"
#import "chrome/browser/ui/cocoa/wrench_menu/recent_tabs_menu_model_delegate.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

using content::HostZoomMap;
using content::UserMetricsAction;

@interface WrenchMenuController (Private)
- (void)createModel;
- (void)adjustPositioning;
- (void)performCommandDispatch:(NSNumber*)tag;
- (NSButton*)zoomDisplay;
- (void)removeAllItems:(NSMenu*)menu;
- (NSMenu*)recentTabsSubmenu;
- (int)maxWidthForMenuModel:(ui::MenuModel*)model
                 modelIndex:(int)modelIndex;
@end

namespace WrenchMenuControllerInternal {

// A C++ delegate that handles the accelerators in the wrench menu.
class AcceleratorDelegate : public ui::AcceleratorProvider {
 public:
  virtual bool GetAcceleratorForCommandId(int command_id,
      ui::Accelerator* out_accelerator) OVERRIDE {
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
                    content::HostZoomMap* map)
      : callback_(base::Bind(&ZoomLevelObserver::OnZoomLevelChanged,
                             base::Unretained(this))),
        controller_(controller),
        map_(map) {
    map_->AddZoomLevelChangedCallback(callback_);
  }

  ~ZoomLevelObserver() {
    map_->RemoveZoomLevelChangedCallback(callback_);
  }

 private:
  void OnZoomLevelChanged(const HostZoomMap::ZoomLevelChange& change) {
    WrenchMenuModel* wrenchMenuModel = [controller_ wrenchMenuModel];
    wrenchMenuModel->UpdateZoomControls();
    const string16 level =
        wrenchMenuModel->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY);
    [[controller_ zoomDisplay] setTitle:SysUTF16ToNSString(level)];
  }

  content::HostZoomMap::ZoomLevelChangedCallback callback_;

  WrenchMenuController* controller_;  // Weak; owns this.
  content::HostZoomMap* map_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(ZoomLevelObserver);
};

}  // namespace WrenchMenuControllerInternal

@implementation WrenchMenuController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;
    observer_.reset(new WrenchMenuControllerInternal::ZoomLevelObserver(
        self, content::HostZoomMap::GetForBrowserContext(browser->profile())));
    acceleratorDelegate_.reset(
        new WrenchMenuControllerInternal::AcceleratorDelegate());
    [self createModel];
  }
  return self;
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model {
  // Non-button item types should be built as normal items.
  ui::MenuModel::ItemType type = model->GetTypeAt(index);
  if (type != ui::MenuModel::TYPE_BUTTON_ITEM) {
    [super addItemToMenu:menu
                 atIndex:index
               fromModel:model];
    return;
  }

  // Handle the special-cased menu items.
  int command_id = model->GetCommandIdAt(index);
  scoped_nsobject<NSMenuItem> customItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:nil
                          keyEquivalent:@""]);
  MenuTrackedRootView* view;
  switch (command_id) {
    case IDC_EDIT_MENU:
      view = [buttonViewController_ editItem];
      DCHECK(view);
      [customItem setView:view];
      [view setMenuItem:customItem];
      break;
    case IDC_ZOOM_MENU:
      view = [buttonViewController_ zoomItem];
      DCHECK(view);
      [customItem setView:view];
      [view setMenuItem:customItem];
      break;
    default:
      NOTREACHED();
      break;
  }
  [self adjustPositioning];
  [menu insertItem:customItem.get() atIndex:index];
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

- (void)menuWillOpen:(NSMenu*)menu {
  [super menuWillOpen:menu];

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

- (void)menuNeedsUpdate:(NSMenu*)menu {
  // First empty out the menu and create a new model.
  [self removeAllItems:menu];
  [self createModel];

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
  ui::MenuModel* model = [self wrenchMenuModel];
  int index = 0;
  if (ui::MenuModel::GetModelAndIndexForCommandId(
          IDC_RESTORE_TAB, &model, &index)) {
    recentTabsMenuModelDelegate_.reset(
        new RecentTabsMenuModelDelegate(model, [self recentTabsSubmenu]));
  }
}

- (void)createModel {
  recentTabsMenuModelDelegate_.reset();
  wrenchMenuModel_.reset(
      new WrenchMenuModel(acceleratorDelegate_.get(), browser_, false, false));
  [self setModel:wrenchMenuModel_.get()];

  buttonViewController_.reset(
      [[WrenchMenuButtonViewController alloc] initWithController:self]);
  [buttonViewController_ view];
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

// This overrdies the parent class to return a custom width for recent tabs
// menu.
- (int)maxWidthForMenuModel:(ui::MenuModel*)model
                 modelIndex:(int)modelIndex {
  int index = 0;
  ui::MenuModel* recentTabsMenuModel = [self wrenchMenuModel];
  if (ui::MenuModel::GetModelAndIndexForCommandId(
          IDC_RESTORE_TAB, &recentTabsMenuModel, &index)) {
    if (recentTabsMenuModel == model) {
      return static_cast<RecentTabsSubMenuModel*>(
          recentTabsMenuModel)->GetMaxWidthForItemAtIndex(modelIndex);
    }
  }
  return -1;
}

@end  // @implementation WrenchMenuController

////////////////////////////////////////////////////////////////////////////////

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

- (id)initWithController:(WrenchMenuController*)controller {
  if ((self = [super initWithNibName:@"WrenchMenu"
                              bundle:base::mac::FrameworkBundle()])) {
    controller_ = controller;
  }
  return self;
}

- (IBAction)dispatchWrenchMenuCommand:(id)sender {
  [controller_ dispatchWrenchMenuCommand:sender];
}

@end  // @implementation WrenchMenuButtonViewController
