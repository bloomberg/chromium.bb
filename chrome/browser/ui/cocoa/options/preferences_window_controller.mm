// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/preferences_window_controller.h"

#include <algorithm>

#include "ui/base/l10n/l10n_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_aedesc.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/policy/managed_prefs_banner_base.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/ui/cocoa/importer/import_settings_dialog.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/options/content_settings_dialog_controller.h"
#import "chrome/browser/ui/cocoa/options/custom_home_pages_model.h"
#import "chrome/browser/ui/cocoa/options/font_language_settings_controller.h"
#import "chrome/browser/ui/cocoa/options/keyword_editor_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/options/search_engine_list_model.h"
#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Colors for the managed preferences warning banner.
static const double kBannerGradientColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
static const double kBannerGradientColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};
static const double kBannerStrokeColor = 135.0 / 255.0;

// Tag id for retrieval via viewWithTag in NSView (from IB).
static const uint32 kBasicsStartupPageTableTag = 1000;

bool IsNewTabUIURLString(const GURL& url) {
  return url == GURL(chrome::kChromeUINewTabURL);
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

// The different behaviors for the "pref group" auto sizing.
enum AutoSizeGroupBehavior {
  kAutoSizeGroupBehaviorVerticalToFit,
  kAutoSizeGroupBehaviorVerticalFirstToFit,
  kAutoSizeGroupBehaviorHorizontalToFit,
  kAutoSizeGroupBehaviorHorizontalFirstGrows,
  kAutoSizeGroupBehaviorFirstTwoAsRowVerticalToFit
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
        NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
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
      NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
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
        NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
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
        NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
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
    case kAutoSizeGroupBehaviorFirstTwoAsRowVerticalToFit: {
      // Start out like kAutoSizeGroupBehaviorVerticalToFit but don't do
      // the first two.  Then handle the two as a row, but apply any
      // vertical shift.
      // All but the first two (in the row); walk bottom up.
      for (NSUInteger index = [views count] - 1; index > 2; --index) {
        NSView* view = [views objectAtIndex:index];
        NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
        DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
        if (localVerticalShift) {
          NSPoint origin = [view frame].origin;
          origin.y += localVerticalShift;
          [view setFrameOrigin:origin];
        }
        localVerticalShift += delta.height;
      }
      // Deal with the two for the horizontal row.  Size the second one.
      CGFloat horizontalShift = 0.0;
      NSView* view = [views objectAtIndex:2];
      NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
      DCHECK_GE(delta.height, 0.0) << "Should NOT shrink in height";
      horizontalShift -= delta.width;
      NSPoint origin = [view frame].origin;
      origin.x += horizontalShift;
      if (localVerticalShift) {
        origin.y += localVerticalShift;
      }
      [view setFrameOrigin:origin];
      // Now expand the first item in the row to consume the space opened up.
      view = [views objectAtIndex:1];
      if (horizontalShift) {
        NSSize delta = NSMakeSize(horizontalShift, 0.0);
        [GTMUILocalizerAndLayoutTweaker
         resizeViewWithoutAutoResizingSubViews:view
                                         delta:delta];
      }
      // And move it up by any amount needed from the previous items.
      if (localVerticalShift) {
        NSPoint origin = [view frame].origin;
        origin.y += localVerticalShift;
        [view setFrameOrigin:origin];
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

// Helper to remove a view and move everything above it down to take over the
// space.
void RemoveViewFromView(NSView* view, NSView* toRemove) {
  // Sort bottom up so we can spin over what is above it.
  NSArray* views =
      [[view subviews] sortedArrayUsingFunction:cocoa_l10n_util::CompareFrameY
                                        context:NULL];

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
// download location as they are horizontal, and should fill the row. Special
// case "Content Settings" and "Clear browsing data" as they are horizontal as
// well.
CGFloat AutoSizeUnderTheHoodContent(NSView* view,
                                    NSPathControl* downloadLocationControl,
                                    NSButton* downloadLocationButton) {
  CGFloat verticalShift = 0.0;

  // Loop bottom up through the views sizing and shifting.
  NSArray* views =
      [[view subviews] sortedArrayUsingFunction:cocoa_l10n_util::CompareFrameY
                                        context:NULL];
  for (NSView* view in views) {
    NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
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
- (void)prefChanged:(std::string*)prefName;
// Callback when sync state has changed.  syncService_ needs to be
// queried to find out what happened.
- (void)syncStateChanged;
// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const UserMetricsAction&) action;
- (void)registerPrefObservers;
- (void)configureInstant;

// KVC setter methods.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)val;
- (void)setHomepageURL:(NSString*)urlString;
- (void)setRestoreOnStartupIndex:(NSInteger)type;
- (void)setShowHomeButton:(BOOL)value;
- (void)setPasswordManagerEnabledIndex:(NSInteger)value;
- (void)setIsUsingDefaultTheme:(BOOL)value;
- (void)setShowAlternateErrorPages:(BOOL)value;
- (void)setUseSuggest:(BOOL)value;
- (void)setDnsPrefetch:(BOOL)value;
- (void)setSafeBrowsing:(BOOL)value;
- (void)setMetricsReporting:(BOOL)value;
- (void)setAskForSaveLocation:(BOOL)value;
- (void)setFileHandlerUIEnabled:(BOOL)value;
- (void)setTranslateEnabled:(BOOL)value;
- (void)setTabsToLinks:(BOOL)value;
- (void)displayPreferenceViewForPage:(OptionsPage)page
                             animate:(BOOL)animate;
- (void)resetSubViews;
- (void)initBannerStateForPage:(OptionsPage)page;

// KVC getter methods.
- (BOOL)fileHandlerUIEnabled;
@end

namespace PreferencesWindowControllerInternal {

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
      [controller_ prefChanged:Details<std::string>(details).ptr()];
  }

  // Overridden from ProfileSyncServiceObserver.
  virtual void OnStateChanged() {
    [controller_ syncStateChanged];
  }

 private:
  PreferencesWindowController* controller_;  // weak, owns us
};

// Tracks state for a managed prefs banner and triggers UI updates through the
// PreferencesWindowController as appropriate.
class ManagedPrefsBannerState : public policy::ManagedPrefsBannerBase {
 public:
  virtual ~ManagedPrefsBannerState() { }

  explicit ManagedPrefsBannerState(PreferencesWindowController* controller,
                                   OptionsPage page,
                                   PrefService* local_state,
                                   PrefService* prefs)
    : policy::ManagedPrefsBannerBase(local_state, prefs, page),
        controller_(controller),
        page_(page) { }

  BOOL IsVisible() {
    return DetermineVisibility();
  }

 protected:
  // Overridden from ManagedPrefsBannerBase.
  virtual void OnUpdateVisibility() {
    [controller_ switchToPage:page_ animate:YES];
  }

 private:
  PreferencesWindowController* controller_;  // weak, owns us
  OptionsPage page_;  // current options page
};

}  // namespace PreferencesWindowControllerInternal

@implementation PreferencesWindowController

@synthesize restoreButtonsEnabled = restoreButtonsEnabled_;
@synthesize restoreURLsEnabled = restoreURLsEnabled_;
@synthesize showHomeButtonEnabled = showHomeButtonEnabled_;
@synthesize defaultSearchEngineEnabled = defaultSearchEngineEnabled_;
@synthesize passwordManagerChoiceEnabled = passwordManagerChoiceEnabled_;
@synthesize passwordManagerButtonEnabled = passwordManagerButtonEnabled_;
@synthesize autoFillSettingsButtonEnabled = autoFillSettingsButtonEnabled_;
@synthesize showAlternateErrorPagesEnabled = showAlternateErrorPagesEnabled_;
@synthesize useSuggestEnabled = useSuggestEnabled_;
@synthesize dnsPrefetchEnabled = dnsPrefetchEnabled_;
@synthesize safeBrowsingEnabled = safeBrowsingEnabled_;
@synthesize metricsReportingEnabled = metricsReportingEnabled_;
@synthesize proxiesConfigureButtonEnabled = proxiesConfigureButtonEnabled_;

- (id)initWithProfile:(Profile*)profile initialPage:(OptionsPage)initialPage {
  DCHECK(profile);
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibPath = [base::mac::MainAppBundle()
                        pathForResource:@"Preferences"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    profile_ = profile->GetOriginalProfile();
    initialPage_ = initialPage;
    prefs_ = profile->GetPrefs();
    DCHECK(prefs_);
    observer_.reset(
        new PreferencesWindowControllerInternal::PrefObserverBridge(self));

    // Set up the model for the custom home page table. The KVO observation
    // tells us when the number of items in the array changes. The normal
    // observation tells us when one of the URLs of an item changes.
    customPagesSource_.reset([[CustomHomePagesModel alloc]
                                initWithProfile:profile_]);
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [customPagesSource_ setURLs:startupPref.urls];

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
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];

    // TODO(akalin): handle incognito profiles?  The windows version of this
    // (in chrome/browser/ui/views/options/content_page_view.cc) just does what
    // we do below.
    syncService_ = profile_->GetProfileSyncService();

    // TODO(akalin): This color is taken from kSyncLabelErrorBgColor in
    // content_page_view.cc.  Either decomp that color out into a
    // function/variable that is referenced by both this file and
    // content_page_view.cc, or maybe pick a more suitable color.
    syncErrorBackgroundColor_.reset(
        [[NSColor colorWithDeviceRed:0xff/255.0
                               green:0x9a/255.0
                                blue:0x9a/255.0
                               alpha:1.0] retain]);

    // Disable the |autoFillSettingsButton_| if we have no
    // |personalDataManager|.
    PersonalDataManager* personalDataManager =
        profile_->GetPersonalDataManager();
    [autoFillSettingsButton_ setHidden:(personalDataManager == NULL)];
    bool autofill_disabled_by_policy =
        autoFillEnabled_.IsManaged() && !autoFillEnabled_.GetValue();
    [self setAutoFillSettingsButtonEnabled:!autofill_disabled_by_policy];
    [self setPasswordManagerChoiceEnabled:!askSavePasswords_.IsManaged()];
    [self setPasswordManagerButtonEnabled:
        !askSavePasswords_.IsManaged() || askSavePasswords_.GetValue()];

    // Initialize the enabled state of the elements on the general tab.
    [self setShowHomeButtonEnabled:!showHomeButton_.IsManaged()];
    [self setEnabledStateOfRestoreOnStartup];
    [self setDefaultSearchEngineEnabled:![searchEngineModel_ isDefaultManaged]];

    // Initialize UI state for the advanced page.
    [self setShowAlternateErrorPagesEnabled:!alternateErrorPages_.IsManaged()];
    [self setUseSuggestEnabled:!useSuggest_.IsManaged()];
    [self setDnsPrefetchEnabled:!dnsPrefetch_.IsManaged()];
    [self setSafeBrowsingEnabled:!safeBrowsing_.IsManaged()];
    [self setMetricsReportingEnabled:!metricsReporting_.IsManaged()];
    proxyPrefs_.reset(
        PrefSetObserver::CreateProxyPrefSetObserver(prefs_, observer_.get()));
    [self setProxiesConfigureButtonEnabled:!proxyPrefs_->IsManaged()];
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

  // There are four problem children within the groups:
  //   Basics - Default Browser
  //   Personal Stuff - Sync
  //   Personal Stuff - Themes
  //   Personal Stuff - Browser Data
  // These four have buttons that with some localizations are wider then the
  // view.  So the four get manually laid out before doing the general work so
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

  [self configureInstant];

  // Size the sync row.
  CGFloat syncRowChange = SizeToFitButtonPair(syncButton_,
                                              syncCustomizeButton_);

  // Size the themes row.
  const NSUInteger kThemeGroupCount = 3;
  const NSUInteger kThemeResetButtonIndex = 1;
  const NSUInteger kThemeThemesButtonIndex = 2;
  DCHECK_EQ([personalStuffGroupThemes_ count], kThemeGroupCount)
      << "Expected only two items in Themes group";
  CGFloat themeRowChange = SizeToFitButtonPair(
      [personalStuffGroupThemes_ objectAtIndex:kThemeResetButtonIndex],
      [personalStuffGroupThemes_ objectAtIndex:kThemeThemesButtonIndex]);

  // Size the Privacy and Clear buttons that make a row in Under the Hood.
  CGFloat privacyRowChange = SizeToFitButtonPair(contentSettingsButton_,
                                                 clearDataButton_);
  // Under the Hood view is narrower (then the other panes) in the nib, subtract
  // out the amount it was already going to grow to match the other panes when
  // calculating how much the row needs things to grow.
  privacyRowChange -=
    ([underTheHoodScroller_ contentSize].width -
     NSWidth([underTheHoodContentView_ frame]));

  // Find the most any row changed in size.
  CGFloat maxWidthChange = std::max(defaultBrowserChange.width, syncRowChange);
  maxWidthChange = std::max(maxWidthChange, themeRowChange);
  maxWidthChange = std::max(maxWidthChange, privacyRowChange);

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
  verticalShift += AutoSizeGroup(
      basicsGroupSearchEngine_,
      kAutoSizeGroupBehaviorFirstTwoAsRowVerticalToFit,
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
  // TODO(akalin): Here we rely on the initial contents of the sync
  // group's text field/link field to be large enough to hold all
  // possible messages so that we don't have to re-layout when sync
  // state changes.  This isn't perfect, since e.g. some sync messages
  // use the user's e-mail address (which may be really long), and the
  // link field is usually not shown (leaving a big empty space).
  // Rethink sync preferences UI for Mac.
  verticalShift += AutoSizeGroup(personalStuffGroupSync_,
                                 kAutoSizeGroupBehaviorVerticalToFit,
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
  NSView* prefsContentView = [prefsWindow contentView];
  NSRect frame = [prefsContentView convertRect:[prefsWindow frame]
                                      fromView:nil];
  frame.size.width = newWidth;
  frame = [prefsContentView convertRect:frame toView:nil];
  [prefsWindow setFrame:frame display:NO];

  // The Under the Hood prefs is a scroller, it shouldn't get any border, so it
  // gets resized to be as wide as the window ended up.
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

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* alertIcon = rb.GetNativeImageNamed(IDR_WARNING);
  DCHECK(alertIcon);
  [managedPrefsBannerWarningImage_ setImage:alertIcon];

  [self initBannerStateForPage:initialPage_];
  [self switchToPage:initialPage_ animate:NO];

  // Save/restore position based on prefs.
  if (g_browser_process && g_browser_process->local_state()) {
    sizeSaver_.reset([[WindowSizeAutosaver alloc]
       initWithWindow:[self window]
          prefService:g_browser_process->local_state()
                 path:prefs::kPreferencesWindowPlacement]);
  }

  // Initialize the banner gradient and stroke color.
  NSColor* bannerStartingColor =
      [NSColor colorWithCalibratedRed:kBannerGradientColorTop[0]
                                green:kBannerGradientColorTop[1]
                                 blue:kBannerGradientColorTop[2]
                                alpha:1.0];
  NSColor* bannerEndingColor =
      [NSColor colorWithCalibratedRed:kBannerGradientColorBottom[0]
                                green:kBannerGradientColorBottom[1]
                                 blue:kBannerGradientColorBottom[2]
                                alpha:1.0];
  scoped_nsobject<NSGradient> bannerGradient(
      [[NSGradient alloc] initWithStartingColor:bannerStartingColor
                                    endingColor:bannerEndingColor]);
  [managedPrefsBannerView_ setGradient:bannerGradient];

  NSColor* bannerStrokeColor =
      [NSColor colorWithCalibratedWhite:kBannerStrokeColor
                                  alpha:1.0];
  [managedPrefsBannerView_ setStrokeColor:bannerStrokeColor];

  // Set accessibility related attributes.
  NSTableView* tableView = [basicsView_ viewWithTag:kBasicsStartupPageTableTag];
  NSString* description =
      l10n_util::GetNSStringWithFixup(IDS_OPTIONS_STARTUP_SHOW_PAGES);
  [tableView accessibilitySetOverrideValue:description
                              forAttribute:NSAccessibilityDescriptionAttribute];
}

- (void)dealloc {
  if (syncService_) {
    syncService_->RemoveObserver(observer_.get());
  }
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [animation_ setDelegate:nil];
  [animation_ stopAnimation];
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
  registrar_.Init(prefs_);
  registrar_.Add(prefs::kURLsToRestoreOnStartup, observer_.get());
  restoreOnStartup_.Init(prefs::kRestoreOnStartup, prefs_, observer_.get());
  newTabPageIsHomePage_.Init(prefs::kHomePageIsNewTabPage,
                             prefs_, observer_.get());
  homepage_.Init(prefs::kHomePage, prefs_, observer_.get());
  showHomeButton_.Init(prefs::kShowHomeButton, prefs_, observer_.get());
  instantEnabled_.Init(prefs::kInstantEnabled, prefs_, observer_.get());

  // Personal Stuff panel
  askSavePasswords_.Init(prefs::kPasswordManagerEnabled,
                         prefs_, observer_.get());
  autoFillEnabled_.Init(prefs::kAutoFillEnabled, prefs_, observer_.get());
  currentTheme_.Init(prefs::kCurrentThemeID, prefs_, observer_.get());

  // Under the hood panel
  alternateErrorPages_.Init(prefs::kAlternateErrorPagesEnabled,
                            prefs_, observer_.get());
  useSuggest_.Init(prefs::kSearchSuggestEnabled, prefs_, observer_.get());
  dnsPrefetch_.Init(prefs::kDnsPrefetchingEnabled, prefs_, observer_.get());
  safeBrowsing_.Init(prefs::kSafeBrowsingEnabled, prefs_, observer_.get());
  autoOpenFiles_.Init(
      prefs::kDownloadExtensionsToOpen, prefs_, observer_.get());
  translateEnabled_.Init(prefs::kEnableTranslate, prefs_, observer_.get());
  tabsToLinks_.Init(prefs::kWebkitTabsToLinks, prefs_, observer_.get());

  // During unit tests, there is no local state object, so we fall back to
  // the prefs object (where we've explicitly registered this pref so we
  // know it's there).
  PrefService* local = g_browser_process->local_state();
  if (!local)
    local = prefs_;
  metricsReporting_.Init(prefs::kMetricsReportingEnabled,
                         local, observer_.get());
  defaultDownloadLocation_.Init(prefs::kDownloadDefaultDirectory, prefs_,
                                observer_.get());
  askForSaveLocation_.Init(prefs::kPromptForDownload, prefs_, observer_.get());

  // We don't need to observe changes in this value.
  lastSelectedPage_.Init(prefs::kOptionsWindowLastTabIndex, local, NULL);
}

// Called when the window wants to be closed.
- (BOOL)windowShouldClose:(id)sender {
  // Stop any animation and clear the delegate to avoid stale pointers.
  [animation_ setDelegate:nil];
  [animation_ stopAnimation];

  return YES;
}

// Called when the user hits the escape key. Closes the window.
- (void)cancel:(id)sender {
  [[self window] performClose:self];
}

// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const UserMetricsAction &)action {
  UserMetrics::RecordAction(action, profile_);
  if (prefs_)
    prefs_->ScheduleSavePersistentPrefs();
}

// Returns the set of keys that |key| depends on for its value so it can be
// re-computed when any of those change as well.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* paths = [super keyPathsForValuesAffectingValueForKey:key];
  if ([key isEqualToString:@"isHomepageURLEnabled"]) {
    paths = [paths setByAddingObject:@"newTabPageIsHomePageIndex"];
    paths = [paths setByAddingObject:@"homepageURL"];
  } else if ([key isEqualToString:@"restoreURLsEnabled"]) {
    paths = [paths setByAddingObject:@"restoreOnStartupIndex"];
  } else if ([key isEqualToString:@"isHomepageChoiceEnabled"]) {
    paths = [paths setByAddingObject:@"newTabPageIsHomePageIndex"];
    paths = [paths setByAddingObject:@"homepageURL"];
  } else if ([key isEqualToString:@"newTabPageIsHomePageIndex"]) {
    paths = [paths setByAddingObject:@"homepageURL"];
  } else if ([key isEqualToString:@"hompageURL"]) {
    paths = [paths setByAddingObject:@"newTabPageIsHomePageIndex"];
  } else if ([key isEqualToString:@"isDefaultBrowser"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserTextColor"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserText"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  }
  return paths;
}

// Launch the Keychain Access app.
- (void)launchKeychainAccess {
  NSString* const kKeychainBundleId = @"com.apple.keychainaccess";
  [[NSWorkspace sharedWorkspace]
      launchAppWithBundleIdentifier:kKeychainBundleId
                            options:0L
     additionalEventParamDescriptor:nil
                   launchIdentifier:nil];
}

//-------------------------------------------------------------------------
// Basics panel

// Sets the home page preferences for kNewTabPageIsHomePage and kHomePage. If a
// blank or null-host URL is passed in we revert to using NewTab page
// as the Home page. Note: using SetValue() causes the observers not to fire,
// which is actually a good thing as we could end up in a state where setting
// the homepage to an empty url would automatically reset the prefs back to
// using the NTP, so we'd be never be able to change it.
- (void)setHomepage:(const GURL&)homepage {
  if (IsNewTabUIURLString(homepage)) {
    newTabPageIsHomePage_.SetValueIfNotManaged(true);
    homepage_.SetValueIfNotManaged(std::string());
  } else if (!homepage.is_valid()) {
    newTabPageIsHomePage_.SetValueIfNotManaged(true);
    if (!homepage.has_host())
      homepage_.SetValueIfNotManaged(std::string());
  } else {
    homepage_.SetValueIfNotManaged(homepage.spec());
  }
}

// Callback when preferences are changed by someone modifying the prefs backend
// externally. |prefName| is the name of the pref that has changed. Unlike on
// Windows, we don't need to use this method for initializing, that's handled by
// Cocoa Bindings.
// Handles prefs for the "Basics" panel.
- (void)basicsPrefChanged:(std::string*)prefName {
  if (*prefName == prefs::kRestoreOnStartup) {
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [self setRestoreOnStartupIndex:startupPref.type];
    [self setEnabledStateOfRestoreOnStartup];
  } else if (*prefName == prefs::kURLsToRestoreOnStartup) {
    [customPagesSource_ reloadURLs];
    [self setEnabledStateOfRestoreOnStartup];
  } else if (*prefName == prefs::kHomePageIsNewTabPage) {
    NSInteger useNewTabPage = newTabPageIsHomePage_.GetValue() ? 0 : 1;
    [self setNewTabPageIsHomePageIndex:useNewTabPage];
  } else if (*prefName == prefs::kHomePage) {
    NSString* value = base::SysUTF8ToNSString(homepage_.GetValue());
    [self setHomepageURL:value];
  } else if (*prefName == prefs::kShowHomeButton) {
    [self setShowHomeButton:showHomeButton_.GetValue() ? YES : NO];
    [self setShowHomeButtonEnabled:!showHomeButton_.IsManaged()];
  } else if (*prefName == prefs::kInstantEnabled) {
    [self configureInstant];
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

// Sets the pref based on the index of the selected cell in the matrix and
// marks the appropriate user metric.
- (void)setRestoreOnStartupIndex:(NSInteger)type {
  SessionStartupPref::Type startupType =
      static_cast<SessionStartupPref::Type>(type);
  switch (startupType) {
    case SessionStartupPref::DEFAULT:
      [self recordUserAction:UserMetricsAction("Options_Startup_Homepage")];
      break;
    case SessionStartupPref::LAST:
      [self recordUserAction:UserMetricsAction("Options_Startup_LastSession")];
      break;
    case SessionStartupPref::URLS:
      [self recordUserAction:UserMetricsAction("Options_Startup_Custom")];
      break;
    default:
      NOTREACHED();
  }
  [self saveSessionStartupWithType:startupType];
}

// Enables or disables the restoreOnStartup elements
- (void) setEnabledStateOfRestoreOnStartup {
  const SessionStartupPref startupPref =
      SessionStartupPref::GetStartupPref(prefs_);
  [self setRestoreButtonsEnabled:!SessionStartupPref::TypeIsManaged(prefs_)];
  [self setRestoreURLsEnabled:!SessionStartupPref::URLsAreManaged(prefs_) &&
      [self restoreOnStartupIndex] == SessionStartupPref::URLS];
}

// Getter for the |customPagesSource| property for bindings.
- (CustomHomePagesModel*)customPagesSource {
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

// Here's a table describing the desired characteristics of the homepage choice
// radio value, it's enabled state and the URL field enabled state. They depend
// on the values of the managed bits for homepage (m_hp) and
// homepageIsNewTabPage (m_ntp) preferences, as well as the value of the
// homepageIsNewTabPage preference (ntp) and whether the homepage preference
// is equal to the new tab page URL (hpisntp).
//
// m_hp m_ntp ntp hpisntp | choice value | choice enabled | URL field enabled
// --------------------------------------------------------------------------
// 0    0     0   0       | homepage     | 1              | 1
// 0    0     0   1       | new tab page | 1              | 0
// 0    0     1   0       | new tab page | 1              | 0
// 0    0     1   1       | new tab page | 1              | 0
// 0    1     0   0       | homepage     | 0              | 1
// 0    1     0   1       | homepage     | 0              | 1
// 0    1     1   0       | new tab page | 0              | 0
// 0    1     1   1       | new tab page | 0              | 0
// 1    0     0   0       | homepage     | 1              | 0
// 1    0     0   1       | new tab page | 0              | 0
// 1    0     1   0       | new tab page | 1              | 0
// 1    0     1   1       | new tab page | 0              | 0
// 1    1     0   0       | homepage     | 0              | 0
// 1    1     0   1       | new tab page | 0              | 0
// 1    1     1   0       | new tab page | 0              | 0
// 1    1     1   1       | new tab page | 0              | 0
//
// thus, we have:
//
//    choice value is new tab page === ntp || (hpisntp && (m_hp || !m_ntp))
//    choice enabled === !m_ntp && !(m_hp && hpisntp)
//    URL field enabled === !ntp && !mhp && !(hpisntp && !m_ntp)
//
// which also make sense if you think about them.

// Checks whether the homepage URL refers to the new tab page.
- (BOOL)isHomepageNewTabUIURL {
  return IsNewTabUIURLString(GURL(homepage_.GetValue().c_str()));
}

// Returns the index of the selected cell in the "home page" marix based on
// the "new tab is home page" pref. Sadly, the ordering is reversed from the
// pref value.
- (NSInteger)newTabPageIsHomePageIndex {
  return newTabPageIsHomePage_.GetValue() ||
      ([self isHomepageNewTabUIURL] &&
          (homepage_.IsManaged() || !newTabPageIsHomePage_.IsManaged())) ?
      kHomepageNewTabPage : kHomepageURL;
}

// Sets the pref based on the given index into the matrix and marks the
// appropriate user metric.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)index {
  bool useNewTabPage = index == kHomepageNewTabPage ? true : false;
  if (useNewTabPage) {
    [self recordUserAction:UserMetricsAction("Options_Homepage_UseNewTab")];
  } else {
    [self recordUserAction:UserMetricsAction("Options_Homepage_UseURL")];
    if ([self isHomepageNewTabUIURL])
      homepage_.SetValueIfNotManaged(std::string());
  }
  newTabPageIsHomePage_.SetValueIfNotManaged(useNewTabPage);
}

// Check whether the new tab and URL homepage radios should be enabled, i.e. if
// the corresponding preference is not managed through configuration policy.
- (BOOL)isHomepageChoiceEnabled {
  return !newTabPageIsHomePage_.IsManaged() &&
      !(homepage_.IsManaged() && [self isHomepageNewTabUIURL]);
}

// Returns whether or not the homepage URL text field should be enabled
// based on if the new tab page is the home page.
- (BOOL)isHomepageURLEnabled {
  return !newTabPageIsHomePage_.GetValue() && !homepage_.IsManaged() &&
      !([self isHomepageNewTabUIURL] && !newTabPageIsHomePage_.IsManaged());
}

// Returns the homepage URL.
- (NSString*)homepageURL {
  NSString* value = base::SysUTF8ToNSString(homepage_.GetValue());
  return [self isHomepageNewTabUIURL] ? nil : value;
}

// Sets the homepage URL to |urlString| with some fixing up.
- (void)setHomepageURL:(NSString*)urlString {
  // If the text field contains a valid URL, sync it to prefs. We run it
  // through the fixer upper to allow input like "google.com" to be converted
  // to something valid ("http://google.com").
  std::string unfixedURL = urlString ? base::SysNSStringToUTF8(urlString) :
                                       chrome::kChromeUINewTabURL;
  [self setHomepage:URLFixerUpper::FixupURL(unfixedURL, std::string())];
}

// Returns whether the home button should be checked based on the preference.
- (BOOL)showHomeButton {
  return showHomeButton_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the home button should be displayed
// based on |value|.
- (void)setShowHomeButton:(BOOL)value {
  if (value)
    [self recordUserAction:UserMetricsAction(
                           "Options_Homepage_ShowHomeButton")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_Homepage_HideHomeButton")];
  showHomeButton_.SetValueIfNotManaged(value ? true : false);
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
  [self recordUserAction:UserMetricsAction("Options_SearchEngineChanged")];
  [searchEngineModel_ setDefaultIndex:index];
}

// Called when the search engine model changes. Update the selection in the
// popup by tickling the bindings with the new value.
- (void)searchEngineModelChanged:(NSNotification*)notify {
  [self setSearchEngineSelectedIndex:[self searchEngineSelectedIndex]];
  [self setDefaultSearchEngineEnabled:![searchEngineModel_ isDefaultManaged]];

}

- (IBAction)manageSearchEngines:(id)sender {
  [KeywordEditorCocoaController showKeywordEditor:profile_];
}

- (IBAction)toggleInstant:(id)sender {
  if (instantEnabled_.GetValue()) {
    InstantController::Disable(profile_);
  } else {
    [instantCheckbox_ setState:NSOffState];
    browser::ShowInstantConfirmDialogIfNecessary([self window], profile_);
  }
}

// Sets the state of the Instant checkbox and adds the type information to the
// label.
- (void)configureInstant {
  bool enabled = instantEnabled_.GetValue();
  NSInteger state = enabled ? NSOnState : NSOffState;
  [instantCheckbox_ setState:state];
}

- (IBAction)learnMoreAboutInstant:(id)sender {
  browser::ShowOptionsURL(profile_, browser::InstantLearnMoreURL());
}

// Called when the user clicks the button to make Chromium the default
// browser. Registers http and https.
- (IBAction)makeDefaultBrowser:(id)sender {
  [self willChangeValueForKey:@"defaultBrowser"];

  ShellIntegration::SetAsDefaultBrowser();
  [self recordUserAction:UserMetricsAction("Options_SetAsDefaultBrowser")];
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
  string16 text =
      l10n_util::GetStringFUTF16(stringId,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  return base::SysUTF16ToNSString(text);
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
- (void)userDataPrefChanged:(std::string*)prefName {
  if (*prefName == prefs::kPasswordManagerEnabled) {
    [self setPasswordManagerEnabledIndex:askSavePasswords_.GetValue() ?
        kEnabledIndex : kDisabledIndex];
    [self setPasswordManagerChoiceEnabled:!askSavePasswords_.IsManaged()];
    [self setPasswordManagerButtonEnabled:
        !askSavePasswords_.IsManaged() || askSavePasswords_.GetValue()];
  }
  if (*prefName == prefs::kAutoFillEnabled) {
    bool autofill_disabled_by_policy =
        autoFillEnabled_.IsManaged() && !autoFillEnabled_.GetValue();
    [self setAutoFillSettingsButtonEnabled:!autofill_disabled_by_policy];
  }
  if (*prefName == prefs::kCurrentThemeID) {
    [self setIsUsingDefaultTheme:currentTheme_.GetValue().length() == 0];
  }
}

// Called to launch the Keychain Access app to show the user's stored
// passwords.
- (IBAction)showSavedPasswords:(id)sender {
  [self recordUserAction:UserMetricsAction("Options_ShowPasswordsExceptions")];
  [self launchKeychainAccess];
}

// Called to show the Auto Fill Settings dialog.
- (IBAction)showAutoFillSettings:(id)sender {
  [self recordUserAction:UserMetricsAction("Options_ShowAutoFillSettings")];

  PersonalDataManager* personalDataManager = profile_->GetPersonalDataManager();
  if (!personalDataManager) {
    // Should not reach here because button is disabled when
    // |personalDataManager| is NULL.
    NOTREACHED();
    return;
  }

  ShowAutoFillDialog(NULL, personalDataManager, profile_);
}

// Called to import data from other browsers (Safari, Firefox, etc).
- (IBAction)importData:(id)sender {
  UserMetrics::RecordAction(UserMetricsAction("Import_ShowDlg"), profile_);
  [ImportSettingsDialogController showImportSettingsDialogForProfile:profile_];
}

- (IBAction)resetThemeToDefault:(id)sender {
  [self recordUserAction:UserMetricsAction("Options_ThemesReset")];
  profile_->ClearTheme();
}

- (IBAction)themesGallery:(id)sender {
  [self recordUserAction:UserMetricsAction("Options_ThemesGallery")];
  Browser* browser = BrowserList::GetLastActive();

  if (!browser || !browser->GetSelectedTabContents())
    browser = Browser::Create(profile_);
  browser->OpenThemeGalleryTabAndActivate();
}

// Called when the "stop syncing" confirmation dialog started by
// doSyncAction is finished.  Stop syncing only If the user clicked
// OK.
- (void)stopSyncAlertDidEnd:(NSAlert*)alert
                 returnCode:(int)returnCode
                contextInfo:(void*)contextInfo {
  DCHECK(syncService_ && !syncService_->IsManaged());
  if (returnCode == NSAlertFirstButtonReturn) {
    syncService_->DisableForUser();
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
  }
}

// Called when the user clicks the multi-purpose sync button in the
// "Personal Stuff" pane.
- (IBAction)doSyncAction:(id)sender {
  DCHECK(syncService_ && !syncService_->IsManaged());
  if (syncService_->HasSyncSetupCompleted()) {
    // If sync setup has completed that means the sync button was a
    // "stop syncing" button.  Bring up a confirmation dialog before
    // actually stopping syncing (see stopSyncAlertDidEnd).
    scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
    [alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
        IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL)];
    [alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
        IDS_CANCEL)];
    [alert setMessageText:l10n_util::GetNSStringWithFixup(
        IDS_SYNC_STOP_SYNCING_DIALOG_TITLE)];
    [alert setInformativeText:l10n_util::GetNSStringWithFixup(
        IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL)];
    [alert setAlertStyle:NSWarningAlertStyle];
    const SEL kEndSelector =
        @selector(stopSyncAlertDidEnd:returnCode:contextInfo:);
    [alert beginSheetModalForWindow:[self window]
                      modalDelegate:self
                     didEndSelector:kEndSelector
                        contextInfo:NULL];
  } else {
    // Otherwise, the sync button was a "sync my bookmarks" button.
    // Kick off the sync setup process.
    syncService_->ShowLoginDialog(NULL);
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
  }
}

// Called when the user clicks on the link to the privacy dashboard.
- (IBAction)showPrivacyDashboard:(id)sender {
  Browser* browser = BrowserList::GetLastActive();

  if (!browser || !browser->GetSelectedTabContents())
    browser = Browser::Create(profile_);
  browser->OpenPrivacyDashboardTabAndActivate();
}

// Called when the user clicks the "Customize Sync" button in the
// "Personal Stuff" pane.  Spawns a dialog-modal sheet that cleans
// itself up on close.
- (IBAction)doSyncCustomize:(id)sender {
  syncService_->ShowConfigure(NULL);
}

// Called when the user clicks on the multi-purpose 'sync problem'
// link.
- (IBAction)doSyncReauthentication:(id)sender {
  DCHECK(syncService_ && !syncService_->IsManaged());
  syncService_->ShowErrorUI(NULL);
}

- (void)setPasswordManagerEnabledIndex:(NSInteger)value {
  if (value == kEnabledIndex)
    [self recordUserAction:UserMetricsAction(
                           "Options_PasswordManager_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_PasswordManager_Disable")];
  askSavePasswords_.SetValueIfNotManaged(value == kEnabledIndex ? true : false);
}

- (NSInteger)passwordManagerEnabledIndex {
  return askSavePasswords_.GetValue() ? kEnabledIndex : kDisabledIndex;
}

- (void)setIsUsingDefaultTheme:(BOOL)value {
  if (value)
    [self recordUserAction:UserMetricsAction(
                           "Options_IsUsingDefaultTheme_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_IsUsingDefaultTheme_Disable")];
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
- (void)underHoodPrefChanged:(std::string*)prefName {
  if (*prefName == prefs::kAlternateErrorPagesEnabled) {
    [self setShowAlternateErrorPages:
        alternateErrorPages_.GetValue() ? YES : NO];
    [self setShowAlternateErrorPagesEnabled:!alternateErrorPages_.IsManaged()];
  }
  else if (*prefName == prefs::kSearchSuggestEnabled) {
    [self setUseSuggest:useSuggest_.GetValue() ? YES : NO];
    [self setUseSuggestEnabled:!useSuggest_.IsManaged()];
  }
  else if (*prefName == prefs::kDnsPrefetchingEnabled) {
    [self setDnsPrefetch:dnsPrefetch_.GetValue() ? YES : NO];
    [self setDnsPrefetchEnabled:!dnsPrefetch_.IsManaged()];
  }
  else if (*prefName == prefs::kSafeBrowsingEnabled) {
    [self setSafeBrowsing:safeBrowsing_.GetValue() ? YES : NO];
    [self setSafeBrowsingEnabled:!safeBrowsing_.IsManaged()];
  }
  else if (*prefName == prefs::kMetricsReportingEnabled) {
    [self setMetricsReporting:metricsReporting_.GetValue() ? YES : NO];
    [self setMetricsReportingEnabled:!metricsReporting_.IsManaged()];
  }
  else if (*prefName == prefs::kDownloadDefaultDirectory) {
    // Poke KVO.
    [self willChangeValueForKey:@"defaultDownloadLocation"];
    [self didChangeValueForKey:@"defaultDownloadLocation"];
  }
  else if (*prefName == prefs::kPromptForDownload) {
    [self setAskForSaveLocation:askForSaveLocation_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kEnableTranslate) {
    [self setTranslateEnabled:translateEnabled_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kWebkitTabsToLinks) {
    [self setTabsToLinks:tabsToLinks_.GetValue() ? YES : NO];
  }
  else if (*prefName == prefs::kDownloadExtensionsToOpen) {
    // Poke KVC.
    [self setFileHandlerUIEnabled:[self fileHandlerUIEnabled]];
  }
  else if (proxyPrefs_->IsObserved(*prefName)) {
    [self setProxiesConfigureButtonEnabled:!proxyPrefs_->IsManaged()];
  }
}

// Set the new download path and notify the UI via KVO.
- (void)downloadPathPanelDidEnd:(NSOpenPanel*)panel
                           code:(NSInteger)returnCode
                        context:(void*)context {
  if (returnCode == NSOKButton) {
    [self recordUserAction:UserMetricsAction("Options_SetDownloadDirectory")];
    NSURL* path = [[panel URLs] lastObject];  // We only allow 1 item.
    [self willChangeValueForKey:@"defaultDownloadLocation"];
    defaultDownloadLocation_.SetValue(base::SysNSStringToUTF8([path path]));
    [self didChangeValueForKey:@"defaultDownloadLocation"];
  }
}

// Bring up an open panel to allow the user to set a new downloads location.
- (void)browseDownloadLocation:(id)sender {
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseFiles:NO];
  [panel setCanChooseDirectories:YES];
  NSString* path = base::SysUTF8ToNSString(defaultDownloadLocation_.GetValue());
  [panel beginSheetForDirectory:path
                           file:nil
                          types:nil
                 modalForWindow:[self window]
                  modalDelegate:self
                 didEndSelector:@selector(downloadPathPanelDidEnd:code:context:)
                    contextInfo:NULL];
}

// Called to clear user's browsing data. This puts up an application-modal
// dialog to guide the user through clearing the data.
- (IBAction)clearData:(id)sender {
  [ClearBrowsingDataController
      showClearBrowsingDialogForProfile:profile_];
}

// Opens the "Content Settings" dialog.
- (IBAction)showContentSettings:(id)sender {
  [ContentSettingsDialogController
      showContentSettingsForType:CONTENT_SETTINGS_TYPE_DEFAULT
                         profile:profile_];
}

- (IBAction)privacyLearnMore:(id)sender {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kPrivacyLearnMoreURL));
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  browser::ShowOptionsURL(profile_, url);
}

- (IBAction)resetAutoOpenFiles:(id)sender {
  profile_->GetDownloadManager()->download_prefs()->ResetAutoOpen();
  [self recordUserAction:UserMetricsAction("Options_ResetAutoOpenFiles")];
}

- (IBAction)openProxyPreferences:(id)sender {
  NSArray* itemsToOpen = [NSArray arrayWithObject:[NSURL fileURLWithPath:
      @"/System/Library/PreferencePanes/Network.prefPane"]];

  const char* proxyPrefCommand = "Proxies";
  base::mac::ScopedAEDesc<> openParams;
  OSStatus status = AECreateDesc('ptru',
                                 proxyPrefCommand,
                                 strlen(proxyPrefCommand),
                                 openParams.OutPointer());
  LOG_IF(ERROR, status != noErr) << "Failed to create open params: " << status;

  LSLaunchURLSpec launchSpec = { 0 };
  launchSpec.itemURLs = (CFArrayRef)itemsToOpen;
  launchSpec.passThruParams = openParams;
  launchSpec.launchFlags = kLSLaunchAsync | kLSLaunchDontAddToRecents;
  LSOpenFromURLSpec(&launchSpec, NULL);
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
    [self recordUserAction:UserMetricsAction(
                           "Options_LinkDoctorCheckbox_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_LinkDoctorCheckbox_Disable")];
  alternateErrorPages_.SetValueIfNotManaged(value ? true : false);
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
    [self recordUserAction:UserMetricsAction(
                           "Options_UseSuggestCheckbox_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_UseSuggestCheckbox_Disable")];
  useSuggest_.SetValueIfNotManaged(value ? true : false);
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
    [self recordUserAction:UserMetricsAction(
                           "Options_DnsPrefetchCheckbox_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_DnsPrefetchCheckbox_Disable")];
  dnsPrefetch_.SetValueIfNotManaged(value ? true : false);
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
    [self recordUserAction:UserMetricsAction(
                           "Options_SafeBrowsingCheckbox_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_SafeBrowsingCheckbox_Disable")];
  safeBrowsing_.SetValueIfNotManaged(value ? true : false);
  SafeBrowsingService* safeBrowsingService =
      g_browser_process->resource_dispatcher_host()->safe_browsing_service();
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(safeBrowsingService,
                        &SafeBrowsingService::OnEnable,
                        safeBrowsing_.GetValue()));
}

// Returns whether the metrics reporting checkbox should be checked based on the
// preference.
- (BOOL)metricsReporting {
  return metricsReporting_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the metrics reporting checkbox
// should be displayed based on |value|.
- (void)setMetricsReporting:(BOOL)value {
  if (value)
    [self recordUserAction:UserMetricsAction(
                           "Options_MetricsReportingCheckbox_Enable")];
  else
    [self recordUserAction:UserMetricsAction(
                           "Options_MetricsReportingCheckbox_Disable")];

  // TODO(pinkerton): windows shows a dialog here telling the user they need to
  // restart for this to take effect. http://crbug.com/34653
  metricsReporting_.SetValueIfNotManaged(value ? true : false);

  bool enabled = metricsReporting_.GetValue();
  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();
  if (enabled != update_pref) {
    DVLOG(1) << "GENERAL SECTION: Unable to set crash report status to "
             << enabled;
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
}

- (NSURL*)defaultDownloadLocation {
  NSString* pathString =
      base::SysUTF8ToNSString(defaultDownloadLocation_.GetValue());
  return [NSURL fileURLWithPath:pathString];
}

- (BOOL)askForSaveLocation {
  return askForSaveLocation_.GetValue();
}

- (void)setAskForSaveLocation:(BOOL)value {
  if (value) {
    [self recordUserAction:UserMetricsAction(
                           "Options_AskForSaveLocation_Enable")];
  } else {
    [self recordUserAction:UserMetricsAction(
                           "Options_AskForSaveLocation_Disable")];
  }
  askForSaveLocation_.SetValue(value);
}

- (BOOL)fileHandlerUIEnabled {
  if (!profile_->GetDownloadManager())  // Not set in unit tests.
    return NO;
  return profile_->GetDownloadManager()->download_prefs()->IsAutoOpenUsed();
}

- (void)setFileHandlerUIEnabled:(BOOL)value {
  [resetFileHandlersButton_ setEnabled:value];
}

- (BOOL)translateEnabled {
  return translateEnabled_.GetValue();
}

- (void)setTranslateEnabled:(BOOL)value {
  if (value) {
    [self recordUserAction:UserMetricsAction("Options_Translate_Enable")];
  } else {
    [self recordUserAction:UserMetricsAction("Options_Translate_Disable")];
  }
  translateEnabled_.SetValue(value);
}

- (BOOL)tabsToLinks {
  return tabsToLinks_.GetValue();
}

- (void)setTabsToLinks:(BOOL)value {
  if (value) {
    [self recordUserAction:UserMetricsAction("Options_TabsToLinks_Enable")];
  } else {
    [self recordUserAction:UserMetricsAction("Options_TabsToLinks_Disable")];
  }
  tabsToLinks_.SetValue(value);
}

- (void)fontAndLanguageEndSheet:(NSWindow*)sheet
                     returnCode:(NSInteger)returnCode
                    contextInfo:(void*)context {
  [sheet close];
  [sheet orderOut:self];
  fontLanguageSettings_ = nil;
}

- (IBAction)changeFontAndLanguageSettings:(id)sender {
  // Intentionally leak the controller as it will clean itself up when the
  // sheet closes.
  fontLanguageSettings_ =
      [[FontLanguageSettingsController alloc] initWithProfile:profile_];
  [NSApp beginSheet:[fontLanguageSettings_ window]
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(fontAndLanguageEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

// Called to launch the Keychain Access app to show the user's stored
// certificates. Note there's no way to script the app to auto-select the
// certificates.
- (IBAction)showCertificates:(id)sender {
  [self recordUserAction:UserMetricsAction("Options_ManagerCerts")];
  [self launchKeychainAccess];
}

- (IBAction)resetToDefaults:(id)sender {
  // The alert will clean itself up in the did-end selector.
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:l10n_util::GetNSString(IDS_OPTIONS_RESET_MESSAGE)];
  NSButton* resetButton = [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_OPTIONS_RESET_OKLABEL)];
  [resetButton setKeyEquivalent:@""];
  NSButton* cancelButton = [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_OPTIONS_RESET_CANCELLABEL)];
  [cancelButton setKeyEquivalent:@"\r"];

  [alert beginSheetModalForWindow:[self window]
                    modalDelegate:self
                   didEndSelector:@selector(resetToDefaults:returned:context:)
                      contextInfo:nil];
}

- (void)resetToDefaults:(NSAlert*)alert
               returned:(NSInteger)code
                context:(void*)context {
  if (code == NSAlertFirstButtonReturn) {
    OptionsUtil::ResetToDefaults(profile_);
  }
  [alert autorelease];
}

//-------------------------------------------------------------------------

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed and should not be NULL.
- (void)prefChanged:(std::string*)prefName {
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
// TODO(akalin): Change the background of the status label/link on error.
- (void)syncStateChanged {
  DCHECK(syncService_);

  string16 statusLabel, linkLabel;
  sync_ui_util::MessageType status =
      sync_ui_util::GetStatusLabels(syncService_, &statusLabel, &linkLabel);
  bool managed = syncService_->IsManaged();

  [syncButton_ setEnabled:!syncService_->WizardIsVisible()];
  NSString* buttonLabel;
  if (syncService_->HasSyncSetupCompleted()) {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
    [syncCustomizeButton_ setHidden:false];
  } else if (syncService_->SetupInProgress()) {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_NTP_SETUP_IN_PROGRESS);
    [syncCustomizeButton_ setHidden:true];
  } else {
    buttonLabel = l10n_util::GetNSStringWithFixup(
        IDS_SYNC_START_SYNC_BUTTON_LABEL);
    [syncCustomizeButton_ setHidden:true];
  }
  [syncCustomizeButton_ setEnabled:!managed];
  [syncButton_ setTitle:buttonLabel];
  [syncButton_ setEnabled:!managed];

  [syncStatus_ setStringValue:base::SysUTF16ToNSString(statusLabel)];
  [syncLink_ setHidden:linkLabel.empty()];
  [syncLink_ setTitle:base::SysUTF16ToNSString(linkLabel)];
  [syncLink_ setEnabled:!managed];

  NSButtonCell* syncLinkCell = static_cast<NSButtonCell*>([syncLink_ cell]);
  if (!syncStatusNoErrorBackgroundColor_) {
    DCHECK(!syncLinkNoErrorBackgroundColor_);
    // We assume that the sync controls start off in a non-error
    // state.
    syncStatusNoErrorBackgroundColor_.reset(
        [[syncStatus_ backgroundColor] retain]);
    syncLinkNoErrorBackgroundColor_.reset(
        [[syncLinkCell backgroundColor] retain]);
  }
  if (status == sync_ui_util::SYNC_ERROR) {
    [syncStatus_ setBackgroundColor:syncErrorBackgroundColor_];
    [syncLinkCell setBackgroundColor:syncErrorBackgroundColor_];
  } else {
    [syncStatus_ setBackgroundColor:syncStatusNoErrorBackgroundColor_];
    [syncLinkCell setBackgroundColor:syncLinkNoErrorBackgroundColor_];
  }
}

// Show the preferences window.
- (IBAction)showPreferences:(id)sender {
  [self showWindow:sender];
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

  // Make sure we aren't being told to display the same thing again.
  if (currentPrefsView_ == prefsView &&
      managedPrefsBannerVisible_ == bannerState_->IsVisible()) {
    return;
  }

  // Remember new options page as current page.
  if (page != OPTIONS_PAGE_DEFAULT)
    lastSelectedPage_.SetValue(page);

  // Stop any running animation, and reset the subviews to the new state. We
  // re-add any views we need for animation later.
  [animation_ stopAnimation];
  NSView* oldPrefsView = currentPrefsView_;
  currentPrefsView_ = prefsView;
  [self resetSubViews];

  // Update the banner state.
  [self initBannerStateForPage:page];
  BOOL showBanner = bannerState_->IsVisible();

  // Update the window title.
  NSToolbarItem* toolbarItem = [self getToolbarItemForPage:page];
  [prefsWindow setTitle:[toolbarItem label]];

  // Calculate new frames for the subviews.
  NSRect prefsViewFrame = [prefsView frame];
  NSRect contentViewFrame = [contentView frame];
  NSRect bannerViewFrame = [managedPrefsBannerView_ frame];

  // Determine what height the managed prefs banner will use.
  CGFloat bannerViewHeight = showBanner ? NSHeight(bannerViewFrame) : 0.0;

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
    prefsViewFrame.origin.y = NSHeight(contentViewFrame) -
        NSHeight(prefsViewFrame) - bannerViewHeight;
  }
  bannerViewFrame.origin.y = NSHeight(prefsViewFrame);
  bannerViewFrame.size.width = NSWidth(contentViewFrame);
  [prefsView setFrame:prefsViewFrame];

  // Figure out the size of the window.
  NSRect windowFrame = [contentView convertRect:[prefsWindow frame]
                                       fromView:nil];
  CGFloat titleToolbarHeight =
      NSHeight(windowFrame) - NSHeight(contentViewFrame);
  windowFrame.size.height =
      NSHeight(prefsViewFrame) + titleToolbarHeight + bannerViewHeight;
  DCHECK_GE(NSWidth(windowFrame), NSWidth(prefsViewFrame))
      << "Initial width set wasn't wide enough.";
  windowFrame = [contentView convertRect:windowFrame toView:nil];
  windowFrame.origin.y = NSMaxY([prefsWindow frame]) - NSHeight(windowFrame);

  // Now change the size.
  if (animate) {
    NSMutableArray* animations = [NSMutableArray arrayWithCapacity:4];
    if (oldPrefsView != prefsView) {
      // Fade between prefs views if they change.
      [contentView addSubview:oldPrefsView
                   positioned:NSWindowBelow
                   relativeTo:nil];
      [animations addObject:
          [NSDictionary dictionaryWithObjectsAndKeys:
              oldPrefsView, NSViewAnimationTargetKey,
              NSViewAnimationFadeOutEffect, NSViewAnimationEffectKey,
              nil]];
      [animations addObject:
          [NSDictionary dictionaryWithObjectsAndKeys:
              prefsView, NSViewAnimationTargetKey,
              NSViewAnimationFadeInEffect, NSViewAnimationEffectKey,
              nil]];
    } else {
      // Make sure the prefs pane ends up in the right position in case we
      // manipulate the banner.
      [animations addObject:
          [NSDictionary dictionaryWithObjectsAndKeys:
              prefsView, NSViewAnimationTargetKey,
              [NSValue valueWithRect:prefsViewFrame],
                  NSViewAnimationEndFrameKey,
              nil]];
    }
    if (showBanner != managedPrefsBannerVisible_) {
      // Slide the warning banner in or out of view.
      [animations addObject:
          [NSDictionary dictionaryWithObjectsAndKeys:
              managedPrefsBannerView_, NSViewAnimationTargetKey,
              [NSValue valueWithRect:bannerViewFrame],
                  NSViewAnimationEndFrameKey,
              nil]];
    }
    // Window resize animation.
    [animations addObject:
        [NSDictionary dictionaryWithObjectsAndKeys:
            prefsWindow, NSViewAnimationTargetKey,
            [NSValue valueWithRect:windowFrame], NSViewAnimationEndFrameKey,
            nil]];
    [animation_ setViewAnimations:animations];
    // The default duration is 0.5s, which actually feels slow in here, so speed
    // it up a bit.
    [animation_ gtm_setDuration:0.2
                      eventMask:NSLeftMouseUpMask];
    [animation_ startAnimation];
  } else {
    // If not animating, odds are we don't want to display either (because it
    // is initial window setup).
    [prefsWindow setFrame:windowFrame display:NO];
    [managedPrefsBannerView_ setFrame:bannerViewFrame];
  }

  managedPrefsBannerVisible_ = showBanner;
}

- (void)resetSubViews {
  // Reset subviews to current prefs view and banner, remove any views that
  // might have been left over from previous state or animation.
  NSArray* subviews = [NSArray arrayWithObjects:
                          currentPrefsView_, managedPrefsBannerView_, nil];
  [[[self window] contentView] setSubviews:subviews];
  [[self window] setInitialFirstResponder:currentPrefsView_];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  DCHECK_EQ(animation_.get(), animation);
  // Animation finished, reset subviews to current prefs view and the banner.
  [self resetSubViews];
}

// Reinitializes the banner state tracker object to watch for managed bits of
// preferences relevant to the given options |page|.
- (void)initBannerStateForPage:(OptionsPage)page {
  page = [self normalizePage:page];

  // During unit tests, there is no local state object, so we fall back to
  // the prefs object (where we've explicitly registered this pref so we
  // know it's there).
  PrefService* local = g_browser_process->local_state();
  if (!local)
    local = prefs_;
  bannerState_.reset(
      new PreferencesWindowControllerInternal::ManagedPrefsBannerState(
          self, page, local, prefs_));
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
  [self autorelease];
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
