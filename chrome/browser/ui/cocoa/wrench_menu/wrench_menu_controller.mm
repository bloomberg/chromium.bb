// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/metrics/user_metrics.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/menu_tracked_root_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

@interface WrenchMenuController (Private)
- (void)adjustPositioning;
- (void)performCommandDispatch:(NSNumber*)tag;
- (NSButton*)zoomDisplay;
@end

namespace WrenchMenuControllerInternal {

class ZoomLevelObserver : public NotificationObserver {
 public:
  explicit ZoomLevelObserver(WrenchMenuController* controller)
      : controller_(controller) {
    registrar_.Add(this, NotificationType::ZOOM_LEVEL_CHANGED,
                   NotificationService::AllSources());
  }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    DCHECK_EQ(type.value, NotificationType::ZOOM_LEVEL_CHANGED);
    WrenchMenuModel* wrenchMenuModel = [controller_ wrenchMenuModel];
    wrenchMenuModel->UpdateZoomControls();
    const string16 level =
        wrenchMenuModel->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY);
    [[controller_ zoomDisplay] setTitle:SysUTF16ToNSString(level)];
  }

 private:
  NotificationRegistrar registrar_;
  WrenchMenuController* controller_;  // Weak; owns this.
};

}  // namespace WrenchMenuControllerInternal

@implementation WrenchMenuController

- (id)init {
  if ((self = [super init])) {
    observer_.reset(new WrenchMenuControllerInternal::ZoomLevelObserver(self));
  }
  return self;
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model
           modelIndex:(int)modelIndex {
  // Non-button item types should be built as normal items.
  ui::MenuModel::ItemType type = model->GetTypeAt(modelIndex);
  if (type != ui::MenuModel::TYPE_BUTTON_ITEM) {
    [super addItemToMenu:menu
                 atIndex:index
               fromModel:model
              modelIndex:modelIndex];
    return;
  }

  // Handle the special-cased menu items.
  int command_id = model->GetCommandIdAt(modelIndex);
  scoped_nsobject<NSMenuItem> customItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:nil
                          keyEquivalent:@""]);
  switch (command_id) {
    case IDC_EDIT_MENU:
      DCHECK(editItem_);
      [customItem setView:editItem_];
      [editItem_ setMenuItem:customItem];
      break;
    case IDC_ZOOM_MENU:
      DCHECK(zoomItem_);
      [customItem setView:zoomItem_];
      [zoomItem_ setMenuItem:customItem];
      break;
    default:
      NOTREACHED();
      break;
  }
  [self adjustPositioning];
  [menu insertItem:customItem.get() atIndex:index];
}

- (NSMenu*)menu {
  NSMenu* menu = [super menu];
  if (![menu delegate]) {
    [menu setDelegate:self];
  }
  return menu;
}

- (void)menuWillOpen:(NSMenu*)menu {
  NSString* title = base::SysUTF16ToNSString(
      [self wrenchMenuModel]->GetLabelForCommandId(IDC_ZOOM_PERCENT_DISPLAY));
  [[zoomItem_ viewWithTag:IDC_ZOOM_PERCENT_DISPLAY] setTitle:title];
  UserMetrics::RecordAction(UserMetricsAction("ShowAppMenu"));

  NSImage* icon = [self wrenchMenuModel]->browser()->window()->IsFullscreen() ?
      [NSImage imageNamed:NSImageNameExitFullScreenTemplate] :
          [NSImage imageNamed:NSImageNameEnterFullScreenTemplate];
  [zoomFullScreen_ setImage:icon];
}

// Used to dispatch commands from the Wrench menu. The custom items within the
// menu cannot be hooked up directly to First Responder because the window in
// which the controls reside is not the BrowserWindowController, but a
// NSCarbonMenuWindow; this screws up the typical |-commandDispatch:| system.
- (IBAction)dispatchWrenchMenuCommand:(id)sender {
  NSInteger tag = [sender tag];
  if (sender == zoomPlus_ || sender == zoomMinus_) {
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
    [self dispatchCommandInternal:tag];
  }
}

- (void)dispatchCommandInternal:(NSInteger)tag {
  // Executing certain commands from the nested run loop of the menu can lead
  // to wonky behavior (e.g. http://crbug.com/49716). To avoid this, schedule
  // the dispatch on the outermost run loop.
  [self performSelector:@selector(performCommandDispatch:)
             withObject:[NSNumber numberWithInt:tag]
             afterDelay:0.0];
}

// Used to perform the actual dispatch on the outermost runloop.
- (void)performCommandDispatch:(NSNumber*)tag {
  [self wrenchMenuModel]->ExecuteCommand([tag intValue]);
}

- (WrenchMenuModel*)wrenchMenuModel {
  return static_cast<WrenchMenuModel*>(model_);
}

// Fit the localized strings into the Cut/Copy/Paste control, then resize the
// whole menu item accordingly.
- (void)adjustPositioning {
  const CGFloat kButtonPadding = 12;
  CGFloat delta = 0;

  // Go through the three buttons from right-to-left, adjusting the size to fit
  // the localized strings while keeping them all aligned on their horizontal
  // edges.
  const size_t kAdjustViewCount = 3;
  NSButton* views[kAdjustViewCount] = { editPaste_, editCopy_, editCut_ };
  for (size_t i = 0; i < kAdjustViewCount; ++i) {
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
  NSRect itemFrame = [editItem_ frame];
  itemFrame.size.width += delta;
  [editItem_ setFrame:itemFrame];

  // Also resize the superview of the buttons, which is an NSView used to slide
  // when the item title is too big and GTM resizes it.
  NSRect parentFrame = [[editCut_ superview] frame];
  parentFrame.size.width += delta;
  parentFrame.origin.x -= delta;
  [[editCut_ superview] setFrame:parentFrame];
}

- (NSButton*)zoomDisplay {
  return zoomDisplay_;
}

@end  // @implementation WrenchMenuController
