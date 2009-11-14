// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/preferences_window_controller.h"

#include <algorithm>
#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/custom_home_pages_model.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#import "chrome/browser/cocoa/search_engine_list_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_status_ui_helper.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_policy.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

NSString* const kUserDoneEditingPrefsNotification =
    @"kUserDoneEditingPrefsNotification";

namespace {

std::wstring GetNewTabUIURLString() {
  std::wstring temp = UTF8ToWide(chrome::kChromeUINewTabURL);
  return URLFixerUpper::FixupURL(temp, std::wstring());
}

// Helper to remove all but the last view from the view heirarchy.
void RemoveAllButLastView(NSArray* views) {
  NSArray* toRemove = [views subarrayWithRange:NSMakeRange(0, [views count]-1)];
  for (NSView* view in toRemove) {
    [view removeFromSuperviewWithoutNeedingDisplay];
  }
}

// Helper that sizes two buttons to fit in a row keeping their spacing, returns
// the total horizontal size change.
CGFloat SizeToFitButtonPair(NSButton* leftButton, NSButton* rightButton) {
  CGFloat widthShift = 0.0;

  NSSize delta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:leftButton];
  DCHECK_EQ(delta.height, 0.0) << "Height changes unsupported";
  widthShift += delta.width;

  if (widthShift != 0.0) {
    NSPoint origin = [rightButton frame].origin;
    origin.x += widthShift;
    [rightButton setFrameOrigin:origin];
  }
  delta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:rightButton];
  DCHECK_EQ(delta.height, 0.0) << "Height changes unsupported";
  widthShift += delta.width;

  return widthShift;
}

// Helper for tweaking the prefs window, if view is a:
//   checkbox, radio group or label: it gets a forced wrap at current size
//   editable field: left as is
//   anything else: do  +[GTMUILocalizerAndLayoutTweaker sizeToFitView:]
NSSize WrapOrSizeToFit(NSView* view) {
  if ([view isKindOfClass:[NSTextField class]]) {
    NSTextField* textField = static_cast<NSTextField*>(view);
    if ([textField isEditable])
      return NSZeroSize;
    CGFloat heightChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
    return NSMakeSize(0.0, heightChange);
  }
  if ([view isKindOfClass:[NSMatrix class]]) {
    NSMatrix* radioGroup = static_cast<NSMatrix*>(view);
    [GTMUILocalizerAndLayoutTweaker wrapRadioGroupForWidth:radioGroup];
    return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
  }
  if ([view isKindOfClass:[NSButton class]]) {
    NSButton* button = static_cast<NSButton*>(view);
    NSButtonCell* buttonCell = [button cell];
    // Decide it's a checkbox via showsStateBy and highlightsBy.
    if (([buttonCell showsStateBy] == NSCellState) &&
        ([buttonCell highlightsBy] == NSCellState)) {
      [GTMUILocalizerAndLayoutTweaker wrapButtonTitleForWidth:button];
      return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
    }
  }
  return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
}

// The different behaviors for the "pref group" auto sizing.
enum AutoSizeGroupBehavior {
  kAutoSizeGroupBehaviorVerticalToFit,
  kAutoSizeGroupBehaviorVerticalFirstToFit,
  kAutoSizeGroupBehaviorHorizontalToFit,
  kAutoSizeGroupBehaviorHorizontalFirstGrows
};

// Helper to tweak the layout of the "pref groups" and also ripple any height
// changes from one group to the next groups' origins.
// |views| is an ordered list of views with first being the label for the
// group and the rest being top down or left to right ordering of the views.
// The label is assumed to already be the same height as all the views it is
// next too.
CGFloat AutoSizeGroup(NSArray* views, AutoSizeGroupBehavior behavior,
                      CGFloat verticalShift) {
  DCHECK_GE([views count], 2U) << "Should be at least a label and a control";
  NSTextField* label = [views objectAtIndex:0];
  DCHECK([label isKindOfClass:[NSTextField class]])
      << "First view should be the label for the group";

  // Auto size the label to see if we need more vertical space for its localized
  // string.
  CGFloat labelHeightChange =
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:label];

  CGFloat localVerticalShift = 0.0;
  switch (behavior) {
    case kAutoSizeGroupBehaviorVerticalToFit: {
      // Walk bottom up doing the sizing and moves.
      for (NSUInteger index = [views count] - 1; index > 0; --index) {
        NSView* view = [views objectAtIndex:index];
        NSSize delta = WrapOrSizeToFit(view);
        DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
        if (localVerticalShift) {
          NSPoint origin = [view frame].origin;
          origin.y += localVerticalShift;
          [view setFrameOrigin:origin];
        }
        localVerticalShift += delta.height;
      }
      break;
    }
    case kAutoSizeGroupBehaviorVerticalFirstToFit: {
      // Just size the top one.
      NSView* view = [views objectAtIndex:1];
      NSSize delta = WrapOrSizeToFit(view);
      DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
      localVerticalShift += delta.height;
      break;
    }
    case kAutoSizeGroupBehaviorHorizontalToFit: {
      // Walk left to right doing the sizing and moves.
      // NOTE: Don't worry about vertical, assume it always fits.
      CGFloat horizontalShift = 0.0;
      NSUInteger count = [views count];
      for (NSUInteger index = 1; index < count; ++index) {
        NSView* view = [views objectAtIndex:index];
        NSSize delta = WrapOrSizeToFit(view);
        DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
        if (horizontalShift) {
          NSPoint origin = [view frame].origin;
          origin.x += horizontalShift;
          [view setFrameOrigin:origin];
        }
        horizontalShift += delta.width;
      }
      break;
    }
    case kAutoSizeGroupBehaviorHorizontalFirstGrows: {
      // Walk right to left doing the sizing and moves, then apply the space
      // collected into the first.
      // NOTE: Don't worry about vertical, assume it always all fits.
      CGFloat horizontalShift = 0.0;
      for (NSUInteger index = [views count] - 1; index > 1; --index) {
        NSView* view = [views objectAtIndex:index];
        NSSize delta = WrapOrSizeToFit(view);
        DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
        horizontalShift -= delta.width;
        NSPoint origin = [view frame].origin;
        origin.x += horizontalShift;
        [view setFrameOrigin:origin];
      }
      if (horizontalShift) {
        NSView* view = [views objectAtIndex:1];
        NSSize delta = NSMakeSize(horizontalShift, 0.0);
        [GTMUILocalizerAndLayoutTweaker
            resizeViewWithoutAutoResizingSubViews:view
                                            delta:delta];
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // If the label grew more then the views, the other views get an extra shift.
  // Otherwise, move the label to its top is aligned with the other views.
  CGFloat nonLabelShift = 0.0;
  if (labelHeightChange > localVerticalShift) {
    // Since the lable is taller, centering the other views looks best, just
    // shift the views by 1/2 of the size difference.
    nonLabelShift = (labelHeightChange - localVerticalShift) / 2.0;
  } else {
    NSPoint origin = [label frame].origin;
    origin.y += localVerticalShift - labelHeightChange;
    [label setFrameOrigin:origin];
  }

  // Apply the input shift requested along with any the shift from label being
  // taller then the rest of the group.
  for (NSView* view in views) {
    NSPoint origin = [view frame].origin;
    origin.y += verticalShift;
    if (view != label) {
      origin.y += nonLabelShift;
    }
    [view setFrameOrigin:origin];
  }

  // Return how much the group grew.
  return localVerticalShift + nonLabelShift;
}

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order bottom up.
NSInteger CompareFrameY(id view1, id view2, void* context) {
  CGFloat y1 = NSMinY([view1 frame]);
  CGFloat y2 = NSMinY([view2 frame]);
  if (y1 < y2)
    return NSOrderedAscending;
  else if (y1 > y2)
    return NSOrderedDescending;
  else
    return NSOrderedSame;
}

// Helper to remove a view and move everything above it down to take over the
// space.
void RemoveViewFromView(NSView* view, NSView* toRemove) {
  // Sort bottom up so we can spin over what is above it.
  NSArray* views =
      [[view subviews] sortedArrayUsingFunction:CompareFrameY context:NULL];

  // Find where |toRemove| was.
  NSUInteger index = [views indexOfObject:toRemove];
  DCHECK_NE(index, NSNotFound);
  NSUInteger count = [views count];
  CGFloat shrinkHeight = 0;
  if (index < (count - 1)) {
    // If we're not the topmost control, the amount to shift is the bottom of
    // |toRemove| to the bottom of the view above it.
    CGFloat shiftDown =
        NSMinY([[views objectAtIndex:index + 1] frame]) -
        NSMinY([toRemove frame]);

    // Now cycle over the views above it moving them down.
    for (++index; index < count; ++index) {
      NSView* view = [views objectAtIndex:index];
      NSPoint origin = [view frame].origin;
      origin.y -= shiftDown;
      [view setFrameOrigin:origin];
    }

    shrinkHeight = shiftDown;
  } else if (index > 0) {
    // If we're the topmost control, there's nothing to shift but we want to
    // shrink until the top edge of the second-topmost control, unless it is
    // actually higher than the topmost control (since we're sorting by the
    // bottom edge).
    shrinkHeight = std::max(0.f,
        NSMaxY([toRemove frame]) -
        NSMaxY([[views objectAtIndex:index - 1] frame]));
  }
  // If we only have one control, don't do any resizing (for now).

  // Remove |toRemove|.
  [toRemove removeFromSuperview];

  [GTMUILocalizerAndLayoutTweaker
      resizeViewWithoutAutoResizingSubViews:view
                                      delta:NSMakeSize(0, -shrinkHeight)];
}

// Simply removes all the views in |toRemove|.
void RemoveGroupFromView(NSView* view, NSArray* toRemove) {
  for (NSView* viewToRemove in toRemove) {
    RemoveViewFromView(view, viewToRemove);
  }
}

// Helper to tweak the layout of the "Under the Hood" content by autosizing all
// the views and moving things up vertically.  Special case the two controls for
// download location as they are horizontal, and should fill the row.
CGFloat AutoSizeUnderTheHoodContent(NSView* view,
                                    NSPathControl* downloadLocationControl,
                                    NSButton* downloadLocationButton) {
  CGFloat verticalShift = 0.0;

  // Loop bottom up through the views sizing and shifting.
  NSArray* views =
      [[view subviews] sortedArrayUsingFunction:CompareFrameY context:NULL];
  for (NSView* view in views) {
    NSSize delta = WrapOrSizeToFit(view);
    DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
    if (verticalShift) {
      NSPoint origin = [view frame].origin;
      origin.y += verticalShift;
      [view setFrameOrigin:origin];
    }
    verticalShift += delta.height;

    // The Download Location controls go in a row with the button aligned to the
    // right edge and the path control using all the rest of the space.
    if (view == downloadLocationButton) {
      NSPoint origin = [downloadLocationButton frame].origin;
      origin.x -= delta.width;
      [downloadLocationButton setFrameOrigin:origin];
      NSSize controlSize = [downloadLocationControl frame].size;
      controlSize.width -= delta.width;
      [downloadLocationControl setFrameSize:controlSize];
    }
  }

  return verticalShift;
}

}  // namespace

//-------------------------------------------------------------------------

@interface PreferencesWindowController(Private)
// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed.
- (void)prefChanged:(std::wstring*)prefName;
// Callback when sync state has changed.  syncService_ needs to be
// queried to find out what happened.
- (void)syncStateChanged;
// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const wchar_t*)action;
- (void)registerPrefObservers;
- (void)unregisterPrefObservers;

- (void)customHomePagesChanged;

// KVC setter methods.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)val;
- (void)setHomepageURL:(NSString*)urlString;
- (void)setRestoreOnStartupIndex:(NSInteger)type;
- (void)setShowHomeButton:(BOOL)value;
- (void)setShowPageOptionsButtons:(BOOL)value;
- (void)setPasswordManagerEnabledIndex:(NSInteger)value;
- (void)setFormAutofillEnabledIndex:(NSInteger)value;
- (void)setIsUsingDefaultTheme:(BOOL)value;
- (void)setShowAlternateErrorPages:(BOOL)value;
- (void)setUseSuggest:(BOOL)value;
- (void)setDnsPrefetch:(BOOL)value;
- (void)setSafeBrowsing:(BOOL)value;
- (void)setMetricsRecording:(BOOL)value;
- (void)setCookieBehavior:(NSInteger)value;
- (void)setAskForSaveLocation:(BOOL)value;
- (void)displayPreferenceViewForPage:(OptionsPage)page
                             animate:(BOOL)animate;
@end

// A C++ class registered for changes in preferences. Bridges the
// notification back to the PWC.
class PrefObserverBridge : public NotificationObserver,
                           public ProfileSyncServiceObserver {
 public:
  PrefObserverBridge(PreferencesWindowController* controller)
      : controller_(controller) {}

  virtual ~PrefObserverBridge() {}

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [controller_ prefChanged:Details<std::wstring>(details).ptr()];
  }

  // Overridden from ProfileSyncServiceObserver.
  virtual void OnStateChanged() {
    [controller_ syncStateChanged];
  }

 private:
  PreferencesWindowController* controller_;  // weak, owns us
};

@implementation PreferencesWindowController

- (id)initWithProfile:(Profile*)profile initialPage:(OptionsPage)initialPage {
  DCHECK(profile);
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibPath = [mac_util::MainAppBundle()
                        pathForResource:@"Preferences"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    profile_ = profile;
    initialPage_ = initialPage;
    prefs_ = profile->GetPrefs();
    DCHECK(prefs_);
    observer_.reset(new PrefObserverBridge(self));

    // Set up the model for the custom home page table. The KVO observation
    // tells us when the number of items in the array changes. The normal
    // observation tells us when one of the URLs of an item changes.
    customPagesSource_.reset([[CustomHomePagesModel alloc]
                                initWithProfile:profile_]);
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [customPagesSource_ setURLs:startupPref.urls];
    [customPagesSource_ addObserver:self
                         forKeyPath:@"customHomePages"
                            options:0L
                            context:NULL];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(homepageEntryChanged:)
               name:kHomepageEntryChangedNotification
             object:nil];

    // Set up the model for the default search popup. Register for notifications
    // about when the model changes so we can update the selection in the view.
    searchEngineModel_.reset(
        [[SearchEngineListModel alloc]
            initWithModel:profile->GetTemplateURLModel()]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(searchEngineModelChanged:)
               name:kSearchEngineListModelChangedNotification
             object:searchEngineModel_.get()];

    // This needs to be done before awakeFromNib: because the bindings set up
    // in the nib rely on it.
    [self registerPrefObservers];

    // Use one animation so we can stop it if the user clicks quickly, and
    // start the new animation.
    animation_.reset([[NSViewAnimation alloc] init]);
    // Make this the delegate so it can remove the old view at the end of the
    // animation (once it is faded out).
    [animation_ setDelegate:self];
    // The default duration is 0.5s, which actually feels slow in here, so speed
    // it up a bit.
    [animation_ gtm_setDuration:0.2];
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];

    // TODO(akalin): handle incognito profiles?  The windows version of this
    // (in chrome/browser/views/options/content_page_view.cc) just does what
    // we do below.
    syncService_ = profile_->GetProfileSyncService();
  }
  return self;
}

- (void)awakeFromNib {

  // Validate some assumptions in debug builds.

  // "Basics", "Personal Stuff", and "Under the Hood" views should be the same
  // width.  They should be the same width so they are laid out to look as good
  // as possible at that width with controls just having to wrap if their text
  // is too long.
  DCHECK_EQ(NSWidth([basicsView_ frame]), NSWidth([personalStuffView_ frame]))
      << "Basics and Personal Stuff should be the same widths";
  DCHECK_EQ(NSWidth([basicsView_ frame]), NSWidth([underTheHoodView_ frame]))
      << "Basics and Under the Hood should be the same widths";
  // "Under the Hood" content should always be skinnier than the scroller it
  // goes into (we resize it).
  DCHECK_LE(NSWidth([underTheHoodContentView_ frame]),
            [underTheHoodScroller_ contentSize].width)
      << "The Under the Hood content should be narrower than the content "
         "of the scroller it goes into";

#if !defined(GOOGLE_CHROME_BUILD)
  // "Enable logging" (breakpad and stats) is only in Google Chrome builds,
  // remove the checkbox and slide everything above it down.
  RemoveViewFromView(underTheHoodContentView_, enableLoggingCheckbox_);
#endif  // !defined(GOOGLE_CHROME_BUILD)

  // There are three problem children within the groups:
  //   Basics - Default Browser
  //   Personal Stuff - Themes
  //   Personal Stuff - Browser Data
  // These three have buttons that with some localizations are wider then the
  // view.  So the three get manually laid out before doing the general work so
  // the views/window can be made wide enough to fit them.  The layout in the
  // general pass is a noop for these buttons (since they are already sized).

  // Size the default browser button.
  const NSUInteger kDefaultBrowserGroupCount = 3;
  const NSUInteger kDefaultBrowserButtonIndex = 1;
  DCHECK_EQ([basicsGroupDefaultBrowser_ count], kDefaultBrowserGroupCount)
      << "Expected only two items in Default Browser group";
  NSButton* defaultBrowserButton =
      [basicsGroupDefaultBrowser_ objectAtIndex:kDefaultBrowserButtonIndex];
  NSSize defaultBrowserChange =
      [GTMUILocalizerAndLayoutTweaker sizeToFitView:defaultBrowserButton];
  DCHECK_EQ(defaultBrowserChange.height, 0.0)
      << "Button should have been right height in nib";

  // Size the themes row.
  const NSUInteger kThemeGroupCount = 3;
  const NSUInteger kThemeResetButtonIndex = 1;
  const NSUInteger kThemeThemesButtonIndex = 2;
  DCHECK_EQ([personalStuffGroupThemes_ count], kThemeGroupCount)
      << "Expected only two items in Themes group";
  CGFloat themeRowChange = SizeToFitButtonPair(
      [personalStuffGroupThemes_ objectAtIndex:kThemeResetButtonIndex],
      [personalStuffGroupThemes_ objectAtIndex:kThemeThemesButtonIndex]);

  // Size the import buttons row.
  const NSUInteger kBrowserDataGroupCount = 4;
  const NSUInteger kBrowserDataImportButtonIndex = 1;
  const NSUInteger kBrowserDataClearButtonIndex = 2;
  DCHECK_EQ([personalStuffGroupBrowserData_ count], kBrowserDataGroupCount)
      << "Expected only two items in Browser Data group";
  CGFloat browserDataRowChange = SizeToFitButtonPair(
      [personalStuffGroupBrowserData_
          objectAtIndex:kBrowserDataImportButtonIndex],
      [personalStuffGroupBrowserData_
          objectAtIndex:kBrowserDataClearButtonIndex]);

  // Find the most any row changed in size.
  CGFloat maxWidthChange = std::max(defaultBrowserChange.width, themeRowChange);
  maxWidthChange = std::max(maxWidthChange, browserDataRowChange);

  // If any grew wider, make the views wider. If they all shrank, they fit the
  // existing view widths, so no change is needed//.
  if (maxWidthChange > 0.0) {
    NSSize viewSize = [basicsView_ frame].size;
    viewSize.width += maxWidthChange;
    [basicsView_ setFrameSize:viewSize];
    viewSize = [personalStuffView_ frame].size;
    viewSize.width += maxWidthChange;
    [personalStuffView_ setFrameSize:viewSize];
  }

  // Now that we have the width needed for Basics and Personal Stuff, lay out
  // those pages bottom up making sure the strings fit and moving things up as
  // needed.

  CGFloat newWidth = NSWidth([basicsView_ frame]);
  CGFloat verticalShift = 0.0;
  verticalShift += AutoSizeGroup(basicsGroupDefaultBrowser_,
                                 kAutoSizeGroupBehaviorVerticalFirstToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(basicsGroupSearchEngine_,
                                 kAutoSizeGroupBehaviorHorizontalFirstGrows,
                                 verticalShift);
  verticalShift += AutoSizeGroup(basicsGroupToolbar_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(basicsGroupHomePage_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(basicsGroupStartup_,
                                 kAutoSizeGroupBehaviorVerticalFirstToFit,
                                 verticalShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeViewWithoutAutoResizingSubViews:basicsView_
                                      delta:NSMakeSize(0.0, verticalShift)];

  verticalShift = 0.0;
  verticalShift += AutoSizeGroup(personalStuffGroupThemes_,
                                 kAutoSizeGroupBehaviorHorizontalToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(personalStuffGroupBrowserData_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(personalStuffGroupAutofill_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
                                 verticalShift);
  verticalShift += AutoSizeGroup(personalStuffGroupPasswords_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
                                 verticalShift);
  // We want to autosize the sync button but not the text field, so we use
  // VerticalFirstToFit.
  verticalShift += AutoSizeGroup(personalStuffGroupSync_,
                                 kAutoSizeGroupBehaviorVerticalFirstToFit,
                                 verticalShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeViewWithoutAutoResizingSubViews:personalStuffView_
                                      delta:NSMakeSize(0.0, verticalShift)];

  if (syncService_) {
    syncService_->AddObserver(observer_.get());
    // Update the controls according to the initial state.
    [self syncStateChanged];
  } else {
    // If sync is disabled we don't want to show the sync controls at all.
    RemoveGroupFromView(personalStuffView_, personalStuffGroupSync_);
  }

  // Make the window as wide as the views.
  NSWindow* prefsWindow = [self window];
  NSRect frame = [prefsWindow frame];
  frame.size.width = newWidth;
  [prefsWindow setFrame:frame display:NO];

  // The Under the Hood prefs is a scroller, it shouldn't get any border, so it
  // gets resized to the as wide as the window ended up.
  NSSize underTheHoodSize = [underTheHoodView_ frame].size;
  underTheHoodSize.width = newWidth;
  [underTheHoodView_ setFrameSize:underTheHoodSize];

  // Widen the Under the Hood content so things can rewrap to the full width.
  NSSize underTheHoodContentSize = [underTheHoodContentView_ frame].size;
  underTheHoodContentSize.width = [underTheHoodScroller_ contentSize].width;
  [underTheHoodContentView_ setFrameSize:underTheHoodContentSize];

  // Now that Under the Hood is the right width, auto-size to the new width to
  // get the final height.
  verticalShift = AutoSizeUnderTheHoodContent(underTheHoodContentView_,
                                              downloadLocationControl_,
                                              downloadLocationButton_);
  [GTMUILocalizerAndLayoutTweaker
      resizeViewWithoutAutoResizingSubViews:underTheHoodContentView_
                                      delta:NSMakeSize(0.0, verticalShift)];
  underTheHoodContentSize = [underTheHoodContentView_ frame].size;

  // Put the Under the Hood content view into the scroller and scroll it to the
  // top.
  [underTheHoodScroller_ setDocumentView:underTheHoodContentView_];
  [underTheHoodContentView_ scrollPoint:
      NSMakePoint(0, underTheHoodContentSize.height)];

  [self switchToPage:initialPage_ animate:NO];

  // TODO(pinkerton): save/restore position based on prefs.
  [[self window] center];
}

- (void)dealloc {
  if (syncService_) {
    syncService_->RemoveObserver(observer_.get());
  }
  [customPagesSource_ removeObserver:self forKeyPath:@"customHomePages"];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self unregisterPrefObservers];
  [super dealloc];
}

// Xcode 3.1.x version of Interface Builder doesn't do a lot for editing
// toolbars in XIB.  So the toolbar's delegate is set to the controller so it
// can tell the toolbar what items are selectable.
- (NSArray*)toolbarSelectableItemIdentifiers:(NSToolbar*)toolbar {
  DCHECK(toolbar == toolbar_);
  return [[toolbar_ items] valueForKey:@"itemIdentifier"];
}

// Register our interest in the preferences we're displaying so if anything
// else in the UI changes them we will be updated.
- (void)registerPrefObservers {
  if (!prefs_) return;

  // Basics panel
  prefs_->AddPrefObserver(prefs::kURLsToRestoreOnStartup, observer_.get());
  restoreOnStartup_.Init(prefs::kRestoreOnStartup, prefs_, observer_.get());
  newTabPageIsHomePage_.Init(prefs::kHomePageIsNewTabPage,
                             prefs_, observer_.get());
  homepage_.Init(prefs::kHomePage, prefs_, observer_.get());
  showHomeButton_.Init(prefs::kShowHomeButton, prefs_, observer_.get());
  showPageOptionButtons_.Init(prefs::kShowPageOptionsButtons, prefs_,
                              observer_.get());
  // TODO(pinkerton): Register Default search.

  // Personal Stuff panel
  askSavePasswords_.Init(prefs::kPasswordManagerEnabled,
                         prefs_, observer_.get());
  formAutofill_.Init(prefs::kFormAutofillEnabled, prefs_, observer_.get());
  currentTheme_.Init(prefs::kCurrentThemeID, prefs_, observer_.get());

  // Under the hood panel
  alternateErrorPages_.Init(prefs::kAlternateErrorPagesEnabled,
                            prefs_, observer_.get());
  useSuggest_.Init(prefs::kSearchSuggestEnabled, prefs_, observer_.get());
  dnsPrefetch_.Init(prefs::kDnsPrefetchingEnabled, prefs_, observer_.get());
  safeBrowsing_.Init(prefs::kSafeBrowsingEnabled, prefs_, observer_.get());

  // During unit tests, there is no local state object, so we fall back to
  // the prefs object (where we've explicitly registered this pref so we
  // know it's there).
  PrefService* local = g_browser_process->local_state();
  if (!local)
    local = prefs_;
  metricsRecording_.Init(prefs::kMetricsReportingEnabled,
                         local, observer_.get());
  cookieBehavior_.Init(prefs::kCookieBehavior, prefs_, observer_.get());
  defaultDownloadLocation_.Init(prefs::kDownloadDefaultDirectory, prefs_,
                                observer_.get());
  askForSaveLocation_.Init(prefs::kPromptForDownload, prefs_, observer_.get());

  // We don't need to observe changes in this value.
  lastSelectedPage_.Init(prefs::kOptionsWindowLastTabIndex, local, NULL);
}

// Clean up what was registered in -registerPrefObservers. We only have to
// clean up the non-PrefMember registrations.
- (void)unregisterPrefObservers {
  if (!prefs_) return;

  // Basics
  prefs_->RemovePrefObserver(prefs::kURLsToRestoreOnStartup, observer_.get());

  // User Data panel
  // Nothing to do here.

  // TODO(pinkerton): do other panels...
}

// Called when a key we're observing via KVO changes.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"customHomePages"]) {
    [self customHomePagesChanged];
    return;
  }
  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

// Called when the user hits the escape key. Closes the window.
- (void)cancel:(id)sender {
  [[self window] performClose:self];
}

// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const wchar_t*)action {
  UserMetrics::RecordComputedAction(action, profile_);
  if (prefs_)
    prefs_->ScheduleSavePersistentPrefs();
}

// Returns the set of keys that |key| depends on for its value so it can be
// re-computed when any of those change as well.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* paths = [super keyPathsForValuesAffectingValueForKey:key];
  if ([key isEqualToString:@"isHomepageURLEnabled"]) {
    paths = [paths setByAddingObject:@"newTabPageIsHomePageIndex"];
  } else if ([key isEqualToString:@"enableRestoreButtons"]) {
    paths = [paths setByAddingObject:@"restoreOnStartupIndex"];
  } else if ([key isEqualToString:@"isDefaultBrowser"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserTextColor"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserText"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  }
  return paths;
}

//-------------------------------------------------------------------------
// Basics panel

// Sets the home page preferences for kNewTabPageIsHomePage and kHomePage. If a
// blank string is passed in we revert to using NewTab page as the Home page.
// When setting the Home Page to NewTab page, we preserve the old value of
// kHomePage (we don't overwrite it). Note: using SetValue() causes the
// observers not to fire, which is actually a good thing as we could end up in a
// state where setting the homepage to an empty url would automatically reset
// the prefs back to using the NTP, so we'd be never be able to change it.
- (void)setHomepage:(const std::wstring&)homepage {
  if (homepage.empty() || homepage == GetNewTabUIURLString()) {
    newTabPageIsHomePage_.SetValue(true);
  } else {
    newTabPageIsHomePage_.SetValue(false);
    homepage_.SetValue(homepage);
  }
}

// Callback when preferences are changed by someone modifying the prefs backend
// externally. |prefName| is the name of the pref that has changed. Unlike on
// Windows, we don't need to use this method for initializing, that's handled by
// Cocoa Bindings.
// Handles prefs for the "Basics" panel.
- (void)basicsPrefChanged:(std::wstring*)prefName {
  if (*prefName == prefs::kRestoreOnStartup) {
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [self setRestoreOnStartupIndex:startupPref.type];
  }

  // TODO(beng): Note that the kURLsToRestoreOnStartup pref is a mutable list,
  //             and changes to mutable lists aren't broadcast through the
  //             observer system, so this condition will
  //             never match. Once support for broadcasting such updates is
  //             added, this will automagically start to work, and this comment
  //             can be removed.
  if (*prefName == prefs::kURLsToRestoreOnStartup) {
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [customPagesSource_ setURLs:startupPref.urls];
  }

  if (*prefName == prefs::kHomePageIsNewTabPage) {
    NSInteger useNewTabPage = newTabPageIsHomePage_.GetValue() ? 0 : 1;
    [self setNewTabPageIsHomePageIndex:useNewTabPage];
  }
  if (*prefName == prefs::kHomePage) {
    NSString* value = base::SysWideToNSString(homepage_.GetValue());
    [self setHomepageURL:value];
  }

  if (*prefName == prefs::kShowHomeButton) {
    [self setShowHomeButton:showHomeButton_.GetValue() ? YES : NO];
  }
  if (*prefName == prefs::kShowPageOptionsButtons) {
    [self setShowPageOptionsButtons:showPageOptionButtons_.GetValue() ?
        YES : NO];
  }
}

// Returns the index of the selected cell in the "on startup" matrix based
// on the "restore on startup" pref. The ordering of the cells is in the
// same order as the pref.
- (NSInteger)restoreOnStartupIndex {
  const SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs_);
  return pref.type;
}

// A helper function that takes the startup session type, grabs the URLs to
// restore, and saves it all in prefs.
- (void)saveSessionStartupWithType:(SessionStartupPref::Type)type {
  SessionStartupPref pref;
  pref.type = type;
  pref.urls = [customPagesSource_.get() URLs];
  SessionStartupPref::SetStartupPref(prefs_, pref);
}

// Called when the custom home pages array changes. Force a save to prefs, but
// in order to save it, we have to look up what the current radio button
// setting is (since they're set together). What a pain.
- (void)customHomePagesChanged {
  const SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs_);
  [self saveSessionStartupWithType:pref.type];
}

// Called when an entry in the custom home page array changes URLs. Force
// a save to prefs.
- (void)homepageEntryChanged:(NSNotification*)notify {
  [self customHomePagesChanged];
}

// Sets the pref based on the index of the selected cell in the matrix and
// marks the appropriate user metric.
- (void)setRestoreOnStartupIndex:(NSInteger)type {
  SessionStartupPref::Type startupType =
      static_cast<SessionStartupPref::Type>(type);
  switch (startupType) {
    case SessionStartupPref::DEFAULT:
      [self recordUserAction:L"Options_Startup_Homepage"];
      break;
    case SessionStartupPref::LAST:
      [self recordUserAction:L"Options_Startup_LastSession"];
      break;
    case SessionStartupPref::URLS:
      [self recordUserAction:L"Options_Startup_Custom"];
      break;
    default:
      NOTREACHED();
  }
  [self saveSessionStartupWithType:startupType];
}

// Returns whether or not the +/-/Current buttons should be enabled, based on
// the current pref value for the startup urls.
- (BOOL)enableRestoreButtons {
  return [self restoreOnStartupIndex] == SessionStartupPref::URLS;
}

// Getter for the |customPagesSource| property for bindings.
- (id)customPagesSource {
  return customPagesSource_.get();
}

// Called when the selection in the table changes. If a flag is set indicating
// that we're waiting for a special select message, edit the cell. Otherwise
// just ignore it, we don't normally care.
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  if (pendingSelectForEdit_) {
    NSTableView* table = [aNotification object];
    NSUInteger selectedRow = [table selectedRow];
    [table editColumn:0 row:selectedRow withEvent:nil select:YES];
    pendingSelectForEdit_ = NO;
  }
}

// Called when the user hits the (+) button for adding a new homepage to the
// list. This will also attempt to make the new item editable so the user can
// just start typing.
- (IBAction)addHomepage:(id)sender {
  [customPagesArrayController_ add:sender];

  // When the new item is added to the model, the array controller will select
  // it. We'll watch for that notification (because we are the table view's
  // delegate) and then make the cell editable. Note that this can't be
  // accomplished simply by subclassing the array controller's add method (I
  // did try). The update of the table is asynchronous with the controller
  // updating the model.
  pendingSelectForEdit_ = YES;
}

// Called when the user hits the (-) button for removing the selected items in
// the homepage table. The controller does all the work.
- (IBAction)removeSelectedHomepages:(id)sender {
  [customPagesArrayController_ remove:sender];
}

// Add all entries for all open browsers with our profile.
- (IBAction)useCurrentPagesAsHomepage:(id)sender {
  std::vector<GURL> urls;
  for (BrowserList::const_iterator browserIter = BrowserList::begin();
       browserIter != BrowserList::end(); ++browserIter) {
    Browser* browser = *browserIter;
    if (browser->profile() != profile_)
      continue;  // Only want entries for open profile.

    for (int tabIndex = 0; tabIndex < browser->tab_count(); ++tabIndex) {
      TabContents* tab = browser->GetTabContentsAt(tabIndex);
      if (tab->ShouldDisplayURL()) {
        const GURL url = browser->GetTabContentsAt(tabIndex)->GetURL();
        if (!url.is_empty())
          urls.push_back(url);
      }
    }
  }
  [customPagesSource_ setURLs:urls];
}

enum { kHomepageNewTabPage, kHomepageURL };

// Returns the index of the selected cell in the "home page" marix based on
// the "new tab is home page" pref. Sadly, the ordering is reversed from the
// pref value.
- (NSInteger)newTabPageIsHomePageIndex {
  return newTabPageIsHomePage_.GetValue() ?
      kHomepageNewTabPage : kHomepageURL;
}

// Sets the pref based on the given index into the matrix and marks the
// appropriate user metric.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)index {
  bool useNewTabPage = index == kHomepageNewTabPage ? true : false;
  if (useNewTabPage)
    [self recordUserAction:L"Options_Homepage_UseNewTab"];
  else
    [self recordUserAction:L"Options_Homepage_UseURL"];
  newTabPageIsHomePage_.SetValue(useNewTabPage);
}

// Returns whether or not the homepage URL text field should be enabled
// based on if the new tab page is the home page.
- (BOOL)isHomepageURLEnabled {
  return newTabPageIsHomePage_.GetValue() ? NO : YES;
}

// Returns the homepage URL.
- (NSString*)homepageURL {
  NSString* value = base::SysWideToNSString(homepage_.GetValue());
  return value;
}

// Sets the homepage URL to |urlString| with some fixing up.
- (void)setHomepageURL:(NSString*)urlString {
  // If the text field contains a valid URL, sync it to prefs. We run it
  // through the fixer upper to allow input like "google.com" to be converted
  // to something valid ("http://google.com").
  if (!urlString)
    urlString = [NSString stringWithFormat:@"%s", chrome::kChromeUINewTabURL];
  std::wstring temp = base::SysNSStringToWide(urlString);
  std::wstring fixedString = URLFixerUpper::FixupURL(temp, std::wstring());
  if (GURL(WideToUTF8(fixedString)).is_valid())
    [self setHomepage:fixedString];
}

// Returns whether the home button should be checked based on the preference.
- (BOOL)showHomeButton {
  return showHomeButton_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the home button should be displayed
// based on |value|.
- (void)setShowHomeButton:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_Homepage_ShowHomeButton"];
  else
    [self recordUserAction:L"Options_Homepage_HideHomeButton"];
  showHomeButton_.SetValue(value ? true : false);
}

// Returns whether the page and options button should be checked based on the
// preference.
- (BOOL)showPageOptionsButtons {
  return showPageOptionButtons_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the page and options buttons should
// be displayed based on |value|.
- (void)setShowPageOptionsButtons:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_Homepage_ShowPageOptionsButtons"];
  else
    [self recordUserAction:L"Options_Homepage_HidePageOptionsButtons"];
  showPageOptionButtons_.SetValue(value ? true : false);
}

// Getter for the |searchEngineModel| property for bindings.
- (id)searchEngineModel {
  return searchEngineModel_.get();
}

// Bindings for the search engine popup. We not binding directly to the model
// in order to siphon off the setter so we can record the metric. If we're
// doing it with one, might as well do it with both.
- (NSUInteger)searchEngineSelectedIndex {
  return [searchEngineModel_ defaultIndex];
}

- (void)setSearchEngineSelectedIndex:(NSUInteger)index {
  [self recordUserAction:L"Options_SearchEngineChanged"];
  [searchEngineModel_ setDefaultIndex:index];
}

// Called when the search engine model changes. Update the selection in the
// popup by tickling the bindings with the new value.
- (void)searchEngineModelChanged:(NSNotification*)notify {
  [self setSearchEngineSelectedIndex:[self searchEngineSelectedIndex]];
}

- (IBAction)manageSearchEngines:(id)sender {
  [KeywordEditorCocoaController showKeywordEditor:profile_];
}

// Called when the user clicks the button to make Chromium the default
// browser. Registers http and https.
- (IBAction)makeDefaultBrowser:(id)sender {
  [self willChangeValueForKey:@"defaultBrowser"];

  ShellIntegration::SetAsDefaultBrowser();
  [self recordUserAction:L"Options_SetAsDefaultBrowser"];
  // If the user made Chrome the default browser, then he/she arguably wants
  // to be notified when that changes.
  prefs_->SetBoolean(prefs::kCheckDefaultBrowser, true);

  // Tickle KVO so that the UI updates.
  [self didChangeValueForKey:@"defaultBrowser"];
}

// Returns the Chromium default browser state.
- (ShellIntegration::DefaultBrowserState)isDefaultBrowser {
  return ShellIntegration::IsDefaultBrowser();
}

// Returns the text color of the "chromium is your default browser" text (green
// for yes, red for no).
- (NSColor*)defaultBrowserTextColor {
  ShellIntegration::DefaultBrowserState state = [self isDefaultBrowser];
  return (state == ShellIntegration::IS_DEFAULT_BROWSER) ?
    [NSColor colorWithCalibratedRed:0.0 green:135.0/255.0 blue:0 alpha:1.0] :
    [NSColor colorWithCalibratedRed:135.0/255.0 green:0 blue:0 alpha:1.0];
}

// Returns the text for the "chromium is your default browser" string dependent
// on if Chromium actually is or not.
- (NSString*)defaultBrowserText {
  ShellIntegration::DefaultBrowserState state = [self isDefaultBrowser];
  int stringId;
  if (state == ShellIntegration::IS_DEFAULT_BROWSER)
    stringId = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  else if (state == ShellIntegration::NOT_DEFAULT_BROWSER)
    stringId = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  else
    stringId = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
  std::wstring text =
      l10n_util::GetStringF(stringId, l10n_util::GetString(IDS_PRODUCT_NAME));
  return base::SysWideToNSString(text);
}

//-------------------------------------------------------------------------
// User Data panel

// Since passwords and forms are radio groups, 'enabled' is index 0 and
// 'disabled' is index 1. Yay.
const int kEnabledIndex = 0;
const int kDisabledIndex = 1;

// Callback when preferences are changed. |prefName| is the name of the pref
// that has changed. Unlike on Windows, we don't need to use this method for
// initializing, that's handled by Cocoa Bindings.
// Handles prefs for the "Personal Stuff" panel.
- (void)userDataPrefChanged:(std::wstring*)prefName {
  if (*prefName == prefs::kPasswordManagerEnabled) {
    [self setPasswordManagerEnabledIndex:askSavePasswords_.GetValue() ?
        kEnabledIndex : kDisabledIndex];
  }
  if (*prefName == prefs::kFormAutofillEnabled) {
    [self setFormAutofillEnabledIndex:formAutofill_.GetValue() ?
        kEnabledIndex : kDisabledIndex];
  }
  if (*prefName == prefs::kCurrentThemeID) {
    [self setIsUsingDefaultTheme:currentTheme_.GetValue().length() == 0];
  }
}

// Called to launch the Keychain Access app to show the user's stored
// passwords.
- (IBAction)showSavedPasswords:(id)sender {
  NSString* const kKeychainBundleId = @"com.apple.keychainaccess";
  [self recordUserAction:L"Options_ShowPasswordsExceptions"];
  [[NSWorkspace sharedWorkspace]
      launchAppWithBundleIdentifier:kKeychainBundleId
                            options:0L
     additionalEventParamDescriptor:nil
                   launchIdentifier:nil];
}

// Called to import data from other browsers (Safari, Firefox, etc).
- (IBAction)importData:(id)sender {
  NOTIMPLEMENTED();
}

// Called to clear user's browsing data. This puts up an application-modal
// dialog to guide the user through clearing the data.
- (IBAction)clearData:(id)sender {
  scoped_nsobject<ClearBrowsingDataController> controller(
      [[ClearBrowsingDataController alloc]
          initWithProfile:profile_]);
  [controller runModalDialog];
}

- (IBAction)resetThemeToDefault:(id)sender {
  [self recordUserAction:L"Options_ThemesReset"];
  profile_->ClearTheme();
}

- (IBAction)themesGallery:(id)sender {
  [self recordUserAction:L"Options_ThemesGallery"];
  Browser* browser =
      BrowserList::FindBrowserWithType(profile_, Browser::TYPE_NORMAL);

  if (!browser || !browser->GetSelectedTabContents()) {
    browser = Browser::Create(profile_);
    browser->OpenURL(
        GURL(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL)),
        GURL(), NEW_WINDOW, PageTransition::LINK);
  } else {
    browser->OpenURL(
        GURL(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL)),
        GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  }
}

- (IBAction)doSyncAction:(id)sender {
  DCHECK(syncService_);
  if (syncService_->HasSyncSetupCompleted()) {
    syncService_->DisableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
    // TODO(akalin): Pop up a confirmation dialog before disabling syncing.
  } else {
    syncService_->EnableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
  }
}

- (void)setPasswordManagerEnabledIndex:(NSInteger)value {
  if (value == kEnabledIndex)
    [self recordUserAction:L"Options_PasswordManager_Enable"];
  else
    [self recordUserAction:L"Options_PasswordManager_Disable"];
  askSavePasswords_.SetValue(value == kEnabledIndex ? true : false);
}

- (NSInteger)passwordManagerEnabledIndex {
  return askSavePasswords_.GetValue() ? kEnabledIndex : kDisabledIndex;
}

- (void)setFormAutofillEnabledIndex:(NSInteger)value {
  if (value == kEnabledIndex)
    [self recordUserAction:L"Options_FormAutofill_Enable"];
  else
    [self recordUserAction:L"Options_FormAutofill_Disable"];
  formAutofill_.SetValue(value == kEnabledIndex ? true : false);
}

- (NSInteger)formAutofillEnabledIndex {
  return formAutofill_.GetValue() ? kEnabledIndex : kDisabledIndex;
}

- (void)setIsUsingDefaultTheme:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_IsUsingDefaultTheme_Enable"];
  else
    [self recordUserAction:L"Options_IsUsingDefaultTheme_Disable"];
}

- (BOOL)isUsingDefaultTheme {
  return currentTheme_.GetValue().length() == 0;
}

//-------------------------------------------------------------------------
// Under the hood panel

// Callback when preferences are changed. |prefName| is the name of the pref
// that has changed. Unlike on Windows, we don't need to use this method for
// initializing, that's handled by Cocoa Bindings.
// Handles prefs for the "Under the hood" panel.
- (void)underHoodPrefChanged:(std::wstring*)prefName {
  if (*prefName == prefs::kAlternateErrorPagesEnabled) {
    [self setShowAlternateErrorPages:
        alternateErrorPages_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kSearchSuggestEnabled) {
    [self setUseSuggest:useSuggest_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kDnsPrefetchingEnabled) {
    [self setDnsPrefetch:dnsPrefetch_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kSafeBrowsingEnabled) {
    [self setSafeBrowsing:safeBrowsing_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kMetricsReportingEnabled) {
    [self setMetricsRecording:metricsRecording_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kCookieBehavior) {
    [self setCookieBehavior:cookieBehavior_.GetValue()];
  }
  else if (*prefName == prefs::kPromptForDownload) {
    [self setAskForSaveLocation:askForSaveLocation_.GetValue() ? YES : NO];
  }
}

// Set the new download path and notify the UI via KVO.
- (void)downloadPathPanelDidEnd:(NSOpenPanel*)panel
                           code:(NSInteger)returnCode
                        context:(void*)context {
  if (returnCode == NSOKButton) {
    [self recordUserAction:L"Options_SetDownloadDirectory"];
    NSURL* path = [[panel URLs] lastObject];  // We only allow 1 item.
    [self willChangeValueForKey:@"defaultDownloadLocation"];
    defaultDownloadLocation_.SetValue(base::SysNSStringToWide([path path]));
    [self didChangeValueForKey:@"defaultDownloadLocation"];
  }
}

// Bring up an open panel to allow the user to set a new downloads location.
- (void)browseDownloadLocation:(id)sender {
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseFiles:NO];
  [panel setCanChooseDirectories:YES];
  NSString* path = base::SysWideToNSString(defaultDownloadLocation_.GetValue());
  [panel beginSheetForDirectory:path
                           file:nil
                          types:nil
                 modalForWindow:[self window]
                  modalDelegate:self
                 didEndSelector:@selector(downloadPathPanelDidEnd:code:context:)
                    contextInfo:NULL];
}

- (IBAction)privacyLearnMore:(id)sender {
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF16(IDS_LEARN_MORE_PRIVACY_URL)),
                   GURL(), NEW_WINDOW, PageTransition::LINK);
}

- (IBAction)toolbarButtonSelected:(id)sender {
  DCHECK([sender isKindOfClass:[NSToolbarItem class]]);
  OptionsPage page = [self getPageForToolbarItem:sender];
  [self displayPreferenceViewForPage:page animate:YES];
}

// Helper to update the window to display a preferences view for a page.
- (void)displayPreferenceViewForPage:(OptionsPage)page
                             animate:(BOOL)animate {
  NSWindow* prefsWindow = [self window];

  // Needs to go *after* the call to [self window], which triggers
  // awakeFromNib if necessary.
  NSView* prefsView = [self getPrefsViewForPage:page];
  NSView* contentView = [prefsWindow contentView];

  // Normally there is only one view, but if the user clicks really quickly, the
  // animation could still been running, and the last view is the one that was
  // animating in.
  NSArray* subviews = [contentView subviews];
  NSView* currentPrefsView = nil;
  if ([subviews count]) {
    currentPrefsView = [subviews lastObject];
  }

  // Make sure we aren't being told to display the same thing again.
  if (currentPrefsView == prefsView) {
    return;
  }

  // Remember new options page as current page.
  if (page != OPTIONS_PAGE_DEFAULT)
    lastSelectedPage_.SetValue(page);

  // Stop any running animation, and remove any past views that were on the way
  // out.
  [animation_ stopAnimation];
  if ([subviews count]) {
    RemoveAllButLastView(subviews);
  }

  NSRect prefsViewFrame = [prefsView frame];
  NSRect contentViewFrame = [contentView frame];
  if (animate) {
    // NSViewAnimation doesn't seem to honor subview resizing as it animates the
    // Window's frame.  So instead of trying to get the top in the right place,
    // just set the origin where it should be at the end, and let the fade/size
    // slide things into the right spot.
    prefsViewFrame.origin.y = 0.0;
  } else {
    // The prefView is anchored to the top of its parent, so set its origin so
    // that the top is where it should be.  When the window's frame is set, the
    // origin will be adjusted to keep it in the right spot.
    prefsViewFrame.origin.y =
        NSHeight(contentViewFrame) - NSHeight(prefsViewFrame);
  }
  [prefsView setFrame:prefsViewFrame];

  // Add the view.
  [contentView addSubview:prefsView];
  [prefsWindow setInitialFirstResponder:prefsView];

  // Update the window title.
  NSToolbarItem* toolbarItem = [self getToolbarItemForPage:page];
  [prefsWindow setTitle:[toolbarItem label]];

  // Figure out the size of the window.
  NSRect windowFrame = [prefsWindow frame];
  CGFloat titleToolbarHeight =
      NSHeight(windowFrame) - NSHeight(contentViewFrame);
  windowFrame.size.height =
      NSHeight(prefsViewFrame) + titleToolbarHeight;
  DCHECK_GE(NSWidth(windowFrame), NSWidth(prefsViewFrame))
      << "Initial width set wasn't wide enough.";
  windowFrame.origin.y = NSMaxY([prefsWindow frame]) - NSHeight(windowFrame);

  // Now change the size.
  if (animate) {
    NSDictionary* oldViewOut =
        [NSDictionary dictionaryWithObjectsAndKeys:
         currentPrefsView, NSViewAnimationTargetKey,
         NSViewAnimationFadeOutEffect, NSViewAnimationEffectKey,
         nil];
    NSDictionary* newViewIn =
        [NSDictionary dictionaryWithObjectsAndKeys:
         prefsView, NSViewAnimationTargetKey,
         NSViewAnimationFadeInEffect, NSViewAnimationEffectKey,
         nil];
    NSDictionary* windowResize =
        [NSDictionary dictionaryWithObjectsAndKeys:
         prefsWindow, NSViewAnimationTargetKey,
         [NSValue valueWithRect:windowFrame], NSViewAnimationEndFrameKey,
         nil];
    [animation_ setViewAnimations:
        [NSArray arrayWithObjects:oldViewOut, newViewIn, windowResize, nil]];
    [animation_ startAnimation];
  } else {
    [currentPrefsView removeFromSuperviewWithoutNeedingDisplay];
    // If not animating, odds are we don't want to display either (because it
    // is initial window setup).
    [prefsWindow setFrame:windowFrame display:NO];
  }
}

- (void)animationDidEnd:(NSAnimation*)animation {
  DCHECK_EQ(animation_.get(), animation);
  // Animation finished, remove everything but the view we just added (it will
  // be last in the list).
  NSArray* subviews = [[[self window] contentView] subviews];
  RemoveAllButLastView(subviews);
}

// Returns whether the alternate error page checkbox should be checked based
// on the preference.
- (BOOL)showAlternateErrorPages {
  return alternateErrorPages_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the alternate error page checkbox
// should be displayed based on |value|.
- (void)setShowAlternateErrorPages:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_LinkDoctorCheckbox_Enable"];
  else
    [self recordUserAction:L"Options_LinkDoctorCheckbox_Disable"];
  alternateErrorPages_.SetValue(value ? true : false);
}

// Returns whether the suggest checkbox should be checked based on the
// preference.
- (BOOL)useSuggest {
  return useSuggest_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the suggest checkbox should be
// displayed based on |value|.
- (void)setUseSuggest:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_UseSuggestCheckbox_Enable"];
  else
    [self recordUserAction:L"Options_UseSuggestCheckbox_Disable"];
  useSuggest_.SetValue(value ? true : false);
}

// Returns whether the DNS prefetch checkbox should be checked based on the
// preference.
- (BOOL)dnsPrefetch {
  return dnsPrefetch_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the DNS prefetch checkbox should be
// displayed based on |value|.
- (void)setDnsPrefetch:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_DnsPrefetchCheckbox_Enable"];
  else
    [self recordUserAction:L"Options_DnsPrefetchCheckbox_Disable"];
  dnsPrefetch_.SetValue(value ? true : false);
  chrome_browser_net::EnableDnsPrefetch(value ? true : false);
}

// Returns whether the safe browsing checkbox should be checked based on the
// preference.
- (BOOL)safeBrowsing {
  return safeBrowsing_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the safe browsing checkbox should be
// displayed based on |value|.
- (void)setSafeBrowsing:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_SafeBrowsingCheckbox_Enable"];
  else
    [self recordUserAction:L"Options_SafeBrowsingCheckbox_Disable"];
  bool enabled = value ? true : false;
  safeBrowsing_.SetValue(enabled);
  SafeBrowsingService* safeBrowsingService =
      g_browser_process->resource_dispatcher_host()->safe_browsing_service();
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      safeBrowsingService, &SafeBrowsingService::OnEnable, enabled));
}

// Returns whether the suggest checkbox should be checked based on the
// preference.
- (BOOL)metricsRecording {
  return metricsRecording_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the suggest checkbox should be
// displayed based on |value|.
- (void)setMetricsRecording:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_MetricsReportingCheckbox_Enable"];
  else
    [self recordUserAction:L"Options_MetricsReportingCheckbox_Disable"];
  bool enabled = value ? true : false;

  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();
  if (enabled != update_pref) {
    DLOG(INFO) <<
        "GENERAL SECTION: Unable to set crash report status to " <<
        enabled;
  }
  // Only change the pref if GoogleUpdateSettings::GetCollectStatsConsent
  // succeeds.
  enabled = update_pref;

  MetricsService* metrics = g_browser_process->metrics_service();
  DCHECK(metrics);
  if (metrics) {
    metrics->SetUserPermitsUpload(enabled);
    if (enabled)
      metrics->Start();
    else
      metrics->Stop();
  }

  // TODO(pinkerton): windows shows a dialog here telling the user they need to
  // restart for this to take effect. Is that really necessary?
  metricsRecording_.SetValue(enabled);
}

// Returns the index of the cookie popup based on the preference.
- (NSInteger)cookieBehavior {
  return cookieBehavior_.GetValue();
}

// Sets the backend pref for whether or not to accept cookies based on |index|.
- (void)setCookieBehavior:(NSInteger)index {
  net::CookiePolicy::Type policy = net::CookiePolicy::ALLOW_ALL_COOKIES;
  if (net::CookiePolicy::ValidType(index))
    policy = net::CookiePolicy::FromInt(index);
  const wchar_t* kUserMetrics[] = {
      L"Options_AllowAllCookies",
      L"Options_BlockThirdPartyCookies",
      L"Options_BlockAllCookies"
  };
  DCHECK(policy >= 0 && (unsigned int)policy < arraysize(kUserMetrics));
  [self recordUserAction:kUserMetrics[policy]];
  cookieBehavior_.SetValue(policy);
}

- (NSURL*)defaultDownloadLocation {
  NSString* pathString =
      base::SysWideToNSString(defaultDownloadLocation_.GetValue());
  return [NSURL fileURLWithPath:pathString];
}

- (BOOL)askForSaveLocation {
  return askForSaveLocation_.GetValue();
}

- (void)setAskForSaveLocation:(BOOL)value {
  if (value) {
    [self recordUserAction:L"Options_AskForSaveLocation_Enable"];
  } else {
    [self recordUserAction:L"Options_AskForSaveLocation_Disable"];
  }
  askForSaveLocation_.SetValue(value);
}

//-------------------------------------------------------------------------

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed and should not be NULL.
- (void)prefChanged:(std::wstring*)prefName {
  DCHECK(prefName);
  if (!prefName) return;
  [self basicsPrefChanged:prefName];
  [self userDataPrefChanged:prefName];
  [self underHoodPrefChanged:prefName];
}

// Callback when sync service state has changed.
//
// TODO(akalin): Decomp this out since a lot of it is copied from the
// Windows version.
// TODO(akalin): Actually have a control for the link.
// TODO(akalin): Change the background of the status label/link on error.
// TODO(akalin): Make sure selecting the "Sync my bookmarks..." menu item
// pops up this preference pane if syncing has already been set up.
- (void)syncStateChanged {
  DCHECK(syncService_);

  [syncButton_ setEnabled:!syncService_->WizardIsVisible()];
  NSString* buttonLabel;
  if (syncService_->HasSyncSetupCompleted()) {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
  } else if (syncService_->SetupInProgress()) {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_NTP_SETUP_IN_PROGRESS);
  } else {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_START_SYNC_BUTTON_LABEL);
  }
  [syncButton_ setTitle:buttonLabel];

  string16 statusLabel, linkLabel;
  SyncStatusUIHelper::GetLabels(syncService_, &statusLabel, &linkLabel);
  [syncStatus_ setStringValue:base::SysUTF16ToNSString(statusLabel)];
}

// Show the preferences window.
- (IBAction)showPreferences:(id)sender {
  [self showWindow:sender];
}

- (void)switchToPage:(OptionsPage)page animate:(BOOL)animate {
  [self displayPreferenceViewForPage:page animate:animate];
  NSToolbarItem* toolbarItem = [self getToolbarItemForPage:page];
  [toolbar_ setSelectedItemIdentifier:[toolbarItem itemIdentifier]];
}

// Called when the window is being closed. Send out a notification that the user
// is done editing preferences. Make sure there are no pending field editors
// by clearing the first responder.
- (void)windowWillClose:(NSNotification*)notification {
  // Setting the first responder to the window ends any in-progress field
  // editor. This will update the model appropriately so there's nothing left
  // to do.
  if (![[self window] makeFirstResponder:[self window]]) {
    // We've hit a recalcitrant field editor, force it to go away.
    [[self window] endEditingFor:nil];
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kUserDoneEditingPrefsNotification
                    object:self];
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
  [customPagesSource_ validateURLs];
}

@end

@implementation PreferencesWindowController(Testing)

- (IntegerPrefMember*)lastSelectedPage {
  return &lastSelectedPage_;
}

- (NSToolbar*)toolbar {
  return toolbar_;
}

- (NSView*)basicsView {
  return basicsView_;
}

- (NSView*)personalStuffView {
  return personalStuffView_;
}

- (NSView*)underTheHoodView {
  return underTheHoodView_;
}

- (OptionsPage)normalizePage:(OptionsPage)page {
  if (page == OPTIONS_PAGE_DEFAULT) {
    // Get the last visited page from local state.
    page = static_cast<OptionsPage>(lastSelectedPage_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT) {
      page = OPTIONS_PAGE_GENERAL;
    }
  }
  return page;
}

- (NSToolbarItem*)getToolbarItemForPage:(OptionsPage)page {
  NSUInteger pageIndex = (NSUInteger)[self normalizePage:page];
  NSArray* items = [toolbar_ items];
  NSUInteger itemCount = [items count];
  DCHECK_GE(pageIndex, 0U);
  if (pageIndex >= itemCount) {
    NOTIMPLEMENTED();
    pageIndex = 0;
  }
  DCHECK_GT(itemCount, 0U);
  return [items objectAtIndex:pageIndex];
}

- (OptionsPage)getPageForToolbarItem:(NSToolbarItem*)toolbarItem {
  // Tags are set in the nib file.
  switch ([toolbarItem tag]) {
    case 0:  // Basics
      return OPTIONS_PAGE_GENERAL;
    case 1:  // Personal Stuff
      return OPTIONS_PAGE_CONTENT;
    case 2:  // Under the Hood
      return OPTIONS_PAGE_ADVANCED;
    default:
      NOTIMPLEMENTED();
      return OPTIONS_PAGE_GENERAL;
  }
}

- (NSView*)getPrefsViewForPage:(OptionsPage)page {
  // The views will be NULL if this is mistakenly called before awakeFromNib.
  DCHECK(basicsView_);
  DCHECK(personalStuffView_);
  DCHECK(underTheHoodView_);
  page = [self normalizePage:page];
  switch (page) {
    case OPTIONS_PAGE_GENERAL:
      return basicsView_;
    case OPTIONS_PAGE_CONTENT:
      return personalStuffView_;
    case OPTIONS_PAGE_ADVANCED:
      return underTheHoodView_;
    case OPTIONS_PAGE_DEFAULT:
    case OPTIONS_PAGE_COUNT:
      LOG(DFATAL) << "Invalid page value " << page;
  }
  return basicsView_;
}

@end
