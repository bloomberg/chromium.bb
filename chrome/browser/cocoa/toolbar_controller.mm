// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// Name of image in the bundle for the yellow of the star icon.
static NSString* const kStarredImageName = @"starred.pdf";

// Height of the toolbar in pixels when the bookmark bar is closed.
static const float kBaseToolbarHeight = 39.0;

// Overlap (in pixels) between the toolbar and the bookmark bar.
static const float kBookmarkBarOverlap = 5.0;

@interface ToolbarController(Private)
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(std::wstring*)prefName;
@end

namespace ToolbarControllerInternal {

// A C++ class registered for changes in preferences. Bridges the
// notification back to the ToolbarController.
class PrefObserverBridge : public NotificationObserver {
 public:
  PrefObserverBridge(ToolbarController* controller)
      : controller_(controller) { }
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [controller_ prefChanged:Details<std::wstring>(details).ptr()];
  }
 private:
  ToolbarController* controller_;  // weak, owns us
};

}  // namespace

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
     resizeDelegate:(id<ViewResizer>)resizeDelegate
   bookmarkDelegate:(id<BookmarkURLOpener>)delegate {
  DCHECK(model && commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:mac_util::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;
    resizeDelegate_ = resizeDelegate;
    bookmarkBarDelegate_ = delegate;
    hasToolbar_ = YES;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(new CommandObserverBridge(self, commands));
    commandObserver_->ObserveCommand(IDC_BACK);
    commandObserver_->ObserveCommand(IDC_FORWARD);
    commandObserver_->ObserveCommand(IDC_RELOAD);
    commandObserver_->ObserveCommand(IDC_HOME);
    commandObserver_->ObserveCommand(IDC_STAR);
  }
  return self;
}

- (void)dealloc {
  // Make sure any code in the base class which assumes [self view] is
  // the "parent" view continues to work.
  hasToolbar_ = YES;

  [super dealloc];
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location
// bar and button state.
- (void)awakeFromNib {
  [self initCommandStatus:commands_];
  locationBarView_.reset(new LocationBarViewMac(locationBar_, commands_,
                                                toolbarModel_, profile_));
  [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];

  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar them based on those prefs.
  prefObserver_.reset(new ToolbarControllerInternal::PrefObserverBridge(self));
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(prefs::kShowHomeButton, prefs, prefObserver_.get());
  showPageOptionButtons_.Init(prefs::kShowPageOptionsButtons, prefs,
                              prefObserver_.get());
  [self showOptionalHomeButton];
  [self showOptionalPageWrenchButtons];

  // Create a sub-controller for the bookmark bar.
  bookmarkBarController_.reset([[BookmarkBarController alloc]
                                   initWithProfile:profile_
                                      initialWidth:NSWidth([[self view] frame])
                                    resizeDelegate:self
                                       urlDelegate:bookmarkBarDelegate_]);

  // Add bookmark bar to the view hierarchy.  This also triggers the
  // nib load.  The bookmark bar is defined (in the nib) to be
  // bottom-aligned to it's parent view (among other things), so
  // position and resize properties don't need to be set.
  [[self view] addSubview:[bookmarkBarController_ view]];
}

- (void)resizeView:(NSView*)view newHeight:(float)height {
  DCHECK(view == [bookmarkBarController_ view]);

  // The bookmark bar is always rooted at the bottom of the toolbar view, with
  // width equal to the toolbar's width.  The toolbar view is resized to
  // accomodate the new bookmark bar height.
  NSRect frame = NSMakeRect(0, 0, [[self view] bounds].size.width, height);
  [view setFrame:frame];

  float newToolbarHeight = kBaseToolbarHeight + height - kBookmarkBarOverlap;
  if (newToolbarHeight < kBaseToolbarHeight)
    newToolbarHeight = kBaseToolbarHeight;

  [resizeDelegate_ resizeView:[self view] newHeight:newToolbarHeight];
}

- (LocationBar*)locationBar {
  return locationBarView_.get();
}

- (void)focusLocationBar {
  if (locationBarView_.get()) {
    locationBarView_->FocusLocation();
  }
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  NSButton* button = nil;
  switch (command) {
    case IDC_BACK:
      button = backButton_;
      break;
    case IDC_FORWARD:
      button = forwardButton_;
      break;
    case IDC_HOME:
      button = homeButton_;
      break;
    case IDC_STAR:
      button = starButton_;
      break;
  }
  [button setEnabled:enabled];
}

// Init the enabled state of the buttons on the toolbar to match the state in
// the controller.
- (void)initCommandStatus:(CommandUpdater*)commands {
  [backButton_ setEnabled:commands->IsCommandEnabled(IDC_BACK) ? YES : NO];
  [forwardButton_
      setEnabled:commands->IsCommandEnabled(IDC_FORWARD) ? YES : NO];
  [reloadButton_ setEnabled:commands->IsCommandEnabled(IDC_RELOAD) ? YES : NO];
  [homeButton_ setEnabled:commands->IsCommandEnabled(IDC_HOME) ? YES : NO];
  [starButton_ setEnabled:commands->IsCommandEnabled(IDC_STAR) ? YES : NO];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  locationBarView_->Update(tab, shouldRestore ? true : false);
}

- (void)setStarredState:(BOOL)isStarred {
  NSImage* starImage = nil;
  if (isStarred)
    starImage = nsimage_cache::ImageNamed(kStarredImageName);

  [(GradientButtonCell*)[starButton_ cell] setUnderlayImage:starImage];
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = @"go_Template.pdf";
  NSInteger tag = IDC_GO;
  if (isLoading) {
    imageName = @"stop_Template.pdf";
    tag = IDC_STOP;
  }
  NSImage* stopStartImage = nsimage_cache::ImageNamed(imageName);
  [stopStartImage setTemplate:YES];
  [goButton_ setImage:stopStartImage];
  [goButton_ setTag:tag];
}

- (void)setHasToolbar:(BOOL)toolbar {
  [self view];  // force nib loading
  hasToolbar_ = toolbar;

  // App mode allows turning off the location bar as well.
  // TODO(???): add more code here when implementing app mode to allow
  // turning off both toolbar AND location bar.

  // TODO(jrg): add mode code to make the location bar NOT editable
  // when in a pop-up.
}

- (NSView*)view {
  if (hasToolbar_)
    return [super view];
  return locationBar_;
}

- (BookmarkBarController*)bookmarkBarController {
  // Browser has a FEATURE_BOOKMARKBAR but it is ignored by Safari
  // when using window.open(); the logic seems to be "if no toolbar,
  // no bookmark bar".
  // TODO(jrg): investigate non-Mac Chrome behavior and possibly expand this.
  if (hasToolbar_ == NO)
    return nil;
  return bookmarkBarController_.get();
}

- (id)customFieldEditorForObject:(id)obj {
  if (obj == locationBar_) {
    // Lazilly construct Field editor, Cocoa UI code always runs on the
    // same thread, so there shoudn't be a race condition here.
    if (autocompleteTextFieldEditor_.get() == nil) {
      autocompleteTextFieldEditor_.reset(
          [[AutocompleteTextFieldEditor alloc] init]);
    }

    // This needs to be called every time, otherwise notifications
    // aren't sent correctly.
    DCHECK(autocompleteTextFieldEditor_.get());
    [autocompleteTextFieldEditor_.get() setFieldEditor:YES];
    return autocompleteTextFieldEditor_.get();
  }
  return nil;
}

// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews {
  return [NSArray arrayWithObjects:backButton_, forwardButton_, reloadButton_,
            homeButton_, starButton_, goButton_, pageButton_, wrenchButton_,
            locationBar_, nil];
}

// Moves |rect| to the right by |delta|, keeping the right side fixed by
// shrinking the width to compensate. Passing a negative value for |deltaX|
// moves to the left and increases the width.
- (NSRect)adjustRect:(NSRect)rect byAmount:(float)deltaX {
  NSRect frame = NSOffsetRect(rect, deltaX, 0);
  frame.size.width -= deltaX;
  return frame;
}

// Computes the padding between the buttons that should have a separation from
// the positions in the nib. Since the forward and reload buttons are always
// visible, we use those buttons as the canonical spacing.
- (float)interButtonSpacing {
  NSRect forwardFrame = [forwardButton_ frame];
  NSRect reloadFrame = [reloadButton_ frame];
  DCHECK(NSMinX(reloadFrame) > NSMaxX(forwardFrame));
  return NSMinX(reloadFrame) - NSMaxX(forwardFrame);
}

// Show or hide the home button based on the pref.
- (void)showOptionalHomeButton {
  BOOL hide = showHomeButton_.GetValue() ? NO : YES;
  if (hide == [homeButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Always shift the star and text field by the width of the home button plus
  // the appropriate gap width. If we're hiding the button, we have to
  // reverse the direction of the movement (to the left).
  float moveX = [self interButtonSpacing] + [homeButton_ frame].size.width;
  if (hide)
    moveX *= -1;  // Reverse the direction of the move.

  [starButton_ setFrame:NSOffsetRect([starButton_ frame], moveX, 0)];
  [locationBar_ setFrame:[self adjustRect:[locationBar_ frame]
                                 byAmount:moveX]];
  [homeButton_ setHidden:hide];
}

// Show or hide the page and wrench buttons based on the pref.
- (void)showOptionalPageWrenchButtons {
  DCHECK([pageButton_ isHidden] == [wrenchButton_ isHidden]);
  BOOL hide = showPageOptionButtons_.GetValue() ? NO : YES;
  if (hide == [pageButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Shift the go button and resize the text field by the width of the
  // page/wrench buttons plus two times the gap width. If we're showing the
  // buttons, we have to reverse the direction of movement (to the left). Unlike
  // the home button above, we only ever have to resize the text field, we don't
  // have to move it.
  float moveX = 2 * [self interButtonSpacing] + NSWidth([pageButton_ frame]) +
                  NSWidth([wrenchButton_ frame]);
  if (!hide)
    moveX *= -1;  // Reverse the direction of the move.
  [goButton_ setFrame:NSOffsetRect([goButton_ frame], moveX, 0)];
  NSRect locationFrame = [locationBar_ frame];
  locationFrame.size.width += moveX;
  [locationBar_ setFrame:locationFrame];

  [pageButton_ setHidden:hide];
  [wrenchButton_ setHidden:hide];
}

- (void)prefChanged:(std::wstring*)prefName {
  if (!prefName) return;
  if (*prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  } else if (*prefName == prefs::kShowPageOptionsButtons) {
    [self showOptionalPageWrenchButtons];
  }
}

- (IBAction)showPageMenu:(id)sender {
  [NSMenu popUpContextMenu:pageMenu_
                 withEvent:[NSApp currentEvent]
                   forView:pageButton_];
}

- (IBAction)showWrenchMenu:(id)sender {
  [NSMenu popUpContextMenu:wrenchMenu_
                 withEvent:[NSApp currentEvent]
                   forView:wrenchButton_];
}

@end
