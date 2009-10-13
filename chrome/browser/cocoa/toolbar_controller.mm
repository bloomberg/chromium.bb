// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/sys_string_conversions.h"
#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/bubble_positioner.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/back_forward_menu_controller.h"
#import "chrome/browser/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/location_bar_view_mac.h"
#import "chrome/browser/cocoa/menu_button.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

// Name of image in the bundle for the yellow of the star icon.
static NSString* const kStarredImageName = @"starred.pdf";

// Height of the toolbar in pixels when the bookmark bar is closed.
static const float kBaseToolbarHeight = 36.0;

// Overlap (in pixels) between the toolbar and the bookmark bar.
static const float kBookmarkBarOverlap = 6.0;

@interface ToolbarController(Private)
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(std::wstring*)prefName;
@end

namespace {

// A C++ class used to correctly position the omnibox.
class BubblePositionerMac : public BubblePositioner {
 public:
  BubblePositionerMac(ToolbarController* controller)
      : controller_(controller) { }
  virtual ~BubblePositionerMac() { }

  // BubblePositioner:
  virtual gfx::Rect GetLocationStackBounds() const {
    return [controller_ locationStackBounds];
  }

 private:
  ToolbarController* controller_;  // weak, owns us
};

}  // namespace

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

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate {
  DCHECK(model && commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:mac_util::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;
    browser_ = browser;
    resizeDelegate_ = resizeDelegate;
    hasToolbar_ = YES;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(new CommandObserverBridge(self, commands));
    commandObserver_->ObserveCommand(IDC_BACK);
    commandObserver_->ObserveCommand(IDC_FORWARD);
    commandObserver_->ObserveCommand(IDC_RELOAD);
    commandObserver_->ObserveCommand(IDC_HOME);
    commandObserver_->ObserveCommand(IDC_BOOKMARK_PAGE);
  }
  return self;
}

- (void)dealloc {
  // Make sure any code in the base class which assumes [self view] is
  // the "parent" view continues to work.
  hasToolbar_ = YES;

  if (trackingArea_.get())
    [[self view] removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location
// bar and button state.
- (void)awakeFromNib {
  [self initCommandStatus:commands_];
  bubblePositioner_.reset(new BubblePositionerMac(self));
  locationBarView_.reset(new LocationBarViewMac(locationBar_,
                                                bubblePositioner_.get(),
                                                commands_, toolbarModel_,
                                                profile_));
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

  // Create the controllers for the back/forward menus.
  backMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_BACK
                   button:backButton_]);
  forwardMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_FORWARD
                   button:forwardButton_]);

  // For a popup window, the toolbar is really just a location bar
  // (see override for [ToolbarController view], below).  When going
  // fullscreen, we remove the toolbar controller's view from the view
  // hierarchy.  Calling [locationBar_ removeFromSuperview] when going
  // fullscreen causes it to get released, making us unhappy
  // (http://crbug.com/18551).  We avoid the problem by incrementing
  // the retain count of the location bar; use of the scoped object
  // helps us remember to release it.
  locationBarRetainer_.reset([locationBar_ retain]);
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:NSZeroRect // Ignored
                                   options:NSTrackingMouseMoved |
                                           NSTrackingInVisibleRect |
                                           NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways
                                     owner:self
                                  userInfo:nil]);
  [[self view] addTrackingArea:trackingArea_.get()];

  // We want a dynamic tooltip on the go button, so tell the go button to ask
  // use for the tooltip
  [goButton_ addToolTipRect:[goButton_ bounds] owner:self userData:nil];

  EncodingMenuControllerDelegate::BuildEncodingMenu(profile_, encodingMenu_);
}

- (void)mouseExited:(NSEvent*)theEvent {
  [[hoveredButton_ cell] setMouseInside:NO animate:YES];
  [hoveredButton_ release];
  hoveredButton_ = nil;
}

- (NSButton*)hoverButtonForEvent:(NSEvent*)theEvent {
  NSButton* targetView = (NSButton*)[[self view]
                                     hitTest:[theEvent locationInWindow]];

  // Only interpret the view as a hoverButton_ if it's both button and has a
  // button cell that cares.  GradientButtonCell derived cells care.
  if (([targetView isKindOfClass:[NSButton class]]) &&
      ([[targetView cell]
         respondsToSelector:@selector(setMouseInside:animate:)]))
    return targetView;
  return nil;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  NSButton* targetView = [self hoverButtonForEvent:theEvent];
  if (hoveredButton_ != targetView) {
    [[hoveredButton_ cell] setMouseInside:NO animate:YES];
    [[targetView cell] setMouseInside:YES animate:YES];
    [hoveredButton_ release];
    hoveredButton_ = [targetView retain];
  }
}

- (void)mouseEntered:(NSEvent*)event {
  [self mouseMoved:event];
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
    case IDC_BOOKMARK_PAGE:
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
  [starButton_
      setEnabled:commands->IsCommandEnabled(IDC_BOOKMARK_PAGE) ? YES : NO];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  locationBarView_->Update(tab, shouldRestore ? true : false);
}

- (void)setStarredState:(BOOL)isStarred {
  NSImage* starImage = nil;
  NSString* toolTip;
  if (isStarred) {
    starImage = nsimage_cache::ImageNamed(kStarredImageName);
    // Cache the string since we'll need it a lot
    static NSString* starredToolTip =
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STARRED) retain];
    toolTip = starredToolTip;
  } else {
    // Cache the string since we'll need it a lot
    static NSString* starToolTip =
        [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STAR) retain];
    toolTip = starToolTip;
  }

  [(GradientButtonCell*)[starButton_ cell] setUnderlayImage:starImage];
  [starButton_ setToolTip:toolTip];
}

- (void)setIsLoading:(BOOL)isLoading {
  NSString* imageName = @"go_Template.pdf";
  NSInteger tag = IDC_GO;
  if (isLoading) {
    imageName = @"stop_Template.pdf";
    tag = IDC_STOP;
  }
  NSImage* stopStartImage = nsimage_cache::ImageNamed(imageName);
  [goButton_ setImage:stopStartImage];
  [goButton_ setTag:tag];
}

- (void)setHasToolbar:(BOOL)toolbar {
  [self view];  // force nib loading
  hasToolbar_ = toolbar;

  // App mode allows turning off the location bar as well.
  // TODO(???): add more code here when implementing app mode to allow
  // turning off both toolbar AND location bar.

  // Make location bar not editable when in a pop-up.
  [locationBar_ setEditable:toolbar];
}

- (NSView*)view {
  if (hasToolbar_)
    return [super view];
  return locationBar_;
}

- (BackgroundGradientView*)backgroundGradientView {
  return (BackgroundGradientView*)[super view];
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
            locationBar_, encodingMenu_, nil];
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
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
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
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
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

- (NSRect)starButtonInWindowCoordinates {
  return [[[starButton_ window] contentView] convertRect:[starButton_ bounds]
                                                fromView:starButton_];
}

- (void)setShouldBeCompressed:(BOOL)compressed {
  CGFloat newToolbarHeight = kBaseToolbarHeight;
  if (compressed)
    newToolbarHeight -= kBookmarkBarOverlap;

  [resizeDelegate_ resizeView:[self view] newHeight:newToolbarHeight];
}

- (NSString*)view:(NSView*)view
 stringForToolTip:(NSToolTipTag)tag
            point:(NSPoint)point
         userData:(void*)userData {
  DCHECK(view == goButton_);

  // Following chrome/browser/views/go_button.cc: GoButton::GetTooltipText()

  // Is it currently 'stop'?
  if ([goButton_ tag] == IDC_STOP) {
    return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STOP);
  }

  // It is 'go', so see what it would do...

  // Fetch the EditView and EditModel
  LocationBar* locationBar = [self locationBar];
  DCHECK(locationBar);
  AutocompleteEditView* editView = locationBar->location_entry();
  DCHECK(editView);
  AutocompleteEditModel* editModel = editView->model();
  DCHECK(editModel);

  std::wstring currentText(editView->GetText());
  if (currentText.empty()) {
    return nil;
  }
  string16 currentText16(WideToUTF16Hack(currentText));

  // It is simply an url it is gonna go to, build the tip with the info.
  if (editModel->CurrentTextIsURL()) {
    return l10n_util::GetNSStringF(IDS_TOOLTIP_GO_SITE, currentText16);
  }

  // Build the tip based on what provide/template it will get.
  std::wstring keyword(editModel->keyword());
  TemplateURLModel* template_url_model =
      editModel->profile()->GetTemplateURLModel();
  const TemplateURL* provider =
      (keyword.empty() || editModel->is_keyword_hint()) ?
      template_url_model->GetDefaultSearchProvider() :
      template_url_model->GetTemplateURLForKeyword(keyword);
  if (!provider)
    return nil;
  std::wstring shortName(provider->AdjustedShortNameForLocaleDirection());
  return l10n_util::GetNSStringF(IDS_TOOLTIP_GO_SEARCH,
                                 WideToUTF16(shortName), currentText16);

}

- (gfx::Rect)locationStackBounds {
  // The number of pixels from the left or right edges of the location stack to
  // "just inside the visible borders".  When the omnibox bubble contents are
  // aligned with this, the visible borders tacked on to the outsides will line
  // up with the visible borders on the location stack.
  const int kLocationStackEdgeWidth = 2;

  NSRect locationFrame = [locationBar_ frame];
  int minX = NSMinX([starButton_ frame]);
  int maxX = NSMaxX([goButton_ frame]);
  DCHECK(minX < NSMinX(locationFrame));
  DCHECK(maxX > NSMaxX(locationFrame));

  NSRect r = NSMakeRect(minX, NSMinY(locationFrame), maxX - minX,
                        NSHeight(locationFrame));
  gfx::Rect stack_bounds(
      NSRectToCGRect([[self view] convertRect:r toView:nil]));
  // Inset the bounds to just inside the visible edges (see comment above).
  stack_bounds.Inset(kLocationStackEdgeWidth, 0);
  return stack_bounds;
}
@end
