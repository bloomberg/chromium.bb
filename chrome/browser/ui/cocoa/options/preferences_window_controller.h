// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/options/options_window.h"

namespace PreferencesWindowControllerInternal {
class PrefObserverBridge;
class ManagedPrefsBannerState;
}

@class CustomHomePagesModel;
@class FontLanguageSettingsController;
class PrefService;
class Profile;
class ProfileSyncService;
@class SearchEngineListModel;
@class VerticalGradientView;
@class WindowSizeAutosaver;

// A window controller that handles the preferences window. The bulk of the
// work is handled via Cocoa Bindings and getter/setter methods that wrap
// cross-platform PrefMember objects. When prefs change in the back-end
// (that is, outside of this UI), our observer receives a notification and can
// tickle the KVO to update the UI so we are always in sync. The bindings are
// specified in the nib file. Preferences are persisted into the back-end
// as they are changed in the UI, and are thus immediately available even while
// the window is still open. When the window closes, a notification is sent
// via the system NotificationCenter. This can be used as a signal to
// release this controller, as it's likely the client wants to enforce there
// only being one (we don't do that internally as it makes it very difficult
// to unit test).
@interface PreferencesWindowController : NSWindowController {
 @private
  Profile* profile_;  // weak ref
  OptionsPage initialPage_;
  PrefService* prefs_;  // weak ref - Obtained from profile_ for convenience.
  // weak ref - Also obtained from profile_ for convenience.  May be NULL.
  ProfileSyncService* syncService_;
  scoped_ptr<PreferencesWindowControllerInternal::PrefObserverBridge>
      observer_;  // Watches for pref changes.
  PrefChangeRegistrar registrar_;  // Manages pref change observer registration.
  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;
  NSView* currentPrefsView_;  // weak ref - current prefs page view.
  scoped_ptr<PreferencesWindowControllerInternal::ManagedPrefsBannerState>
      bannerState_;
  BOOL managedPrefsBannerVisible_;

  IBOutlet NSToolbar* toolbar_;
  IBOutlet VerticalGradientView* managedPrefsBannerView_;
  IBOutlet NSImageView* managedPrefsBannerWarningImage_;

  // The views we'll rotate through
  IBOutlet NSView* basicsView_;
  IBOutlet NSView* personalStuffView_;
  IBOutlet NSView* underTheHoodView_;
  // The last page the user was on when they opened the Options window.
  IntegerPrefMember lastSelectedPage_;

  // The groups of the Basics view for layout fixup.
  IBOutlet NSArray* basicsGroupStartup_;
  IBOutlet NSArray* basicsGroupHomePage_;
  IBOutlet NSArray* basicsGroupToolbar_;
  IBOutlet NSArray* basicsGroupSearchEngine_;
  IBOutlet NSArray* basicsGroupDefaultBrowser_;

  // The groups of the Personal Stuff view for layout fixup.
  IBOutlet NSArray* personalStuffGroupSync_;
  IBOutlet NSArray* personalStuffGroupPasswords_;
  IBOutlet NSArray* personalStuffGroupAutofill_;
  IBOutlet NSArray* personalStuffGroupBrowserData_;
  IBOutlet NSArray* personalStuffGroupThemes_;

  // Having two animations around is bad (they fight), so just use one.
  scoped_nsobject<NSViewAnimation> animation_;

  IBOutlet NSArrayController* customPagesArrayController_;

  // Basics panel
  IntegerPrefMember restoreOnStartup_;
  scoped_nsobject<CustomHomePagesModel> customPagesSource_;
  BooleanPrefMember newTabPageIsHomePage_;
  StringPrefMember homepage_;
  BooleanPrefMember showHomeButton_;
  BooleanPrefMember instantEnabled_;
  IBOutlet NSButton* instantCheckbox_;
  scoped_nsobject<SearchEngineListModel> searchEngineModel_;
  BooleanPrefMember default_browser_policy_;
  // Used when creating a new home page url to make the new cell editable.
  BOOL pendingSelectForEdit_;
  BOOL restoreButtonsEnabled_;
  BOOL restoreURLsEnabled_;
  BOOL showHomeButtonEnabled_;
  BOOL defaultSearchEngineEnabled_;

  // User Data panel
  BooleanPrefMember askSavePasswords_;
  BooleanPrefMember autoFillEnabled_;
  IBOutlet NSButton* autoFillSettingsButton_;
  IBOutlet NSButton* syncButton_;
  IBOutlet NSButton* syncCustomizeButton_;
  IBOutlet NSTextField* syncStatus_;
  IBOutlet NSButton* syncLink_;
  IBOutlet NSButton* privacyDashboardLink_;
  scoped_nsobject<NSColor> syncStatusNoErrorBackgroundColor_;
  scoped_nsobject<NSColor> syncLinkNoErrorBackgroundColor_;
  scoped_nsobject<NSColor> syncErrorBackgroundColor_;
  BOOL passwordManagerChoiceEnabled_;
  BOOL passwordManagerButtonEnabled_;
  BOOL autoFillSettingsButtonEnabled_;

  // Under the hood panel
  IBOutlet NSView* underTheHoodContentView_;
  IBOutlet NSScrollView* underTheHoodScroller_;
  IBOutlet NSButton* contentSettingsButton_;
  IBOutlet NSButton* clearDataButton_;
  BooleanPrefMember alternateErrorPages_;
  BooleanPrefMember useSuggest_;
  BooleanPrefMember dnsPrefetch_;
  BooleanPrefMember safeBrowsing_;
  BooleanPrefMember metricsReporting_;
  IBOutlet NSPathControl* downloadLocationControl_;
  IBOutlet NSButton* downloadLocationButton_;
  StringPrefMember defaultDownloadLocation_;
  BooleanPrefMember askForSaveLocation_;
  IBOutlet NSButton* resetFileHandlersButton_;
  StringPrefMember autoOpenFiles_;
  BooleanPrefMember translateEnabled_;
  BooleanPrefMember tabsToLinks_;
  FontLanguageSettingsController* fontLanguageSettings_;
  StringPrefMember currentTheme_;
  IBOutlet NSButton* enableLoggingCheckbox_;
  scoped_ptr<PrefSetObserver> proxyPrefs_;
  BOOL showAlternateErrorPagesEnabled_;
  BOOL useSuggestEnabled_;
  BOOL dnsPrefetchEnabled_;
  BOOL safeBrowsingEnabled_;
  BOOL metricsReportingEnabled_;
  BOOL proxiesConfigureButtonEnabled_;
}

// Usable from cocoa bindings to hook up the custom home pages table.
@property(nonatomic, readonly) CustomHomePagesModel* customPagesSource;

// Properties for the enabled state of various UI elements. Keep these ordered
// by occurrence on the dialog.
@property(nonatomic) BOOL restoreButtonsEnabled;
@property(nonatomic) BOOL restoreURLsEnabled;
@property(nonatomic) BOOL showHomeButtonEnabled;
@property(nonatomic) BOOL defaultSearchEngineEnabled;
@property(nonatomic) BOOL passwordManagerChoiceEnabled;
@property(nonatomic) BOOL passwordManagerButtonEnabled;
@property(nonatomic) BOOL autoFillSettingsButtonEnabled;
@property(nonatomic) BOOL showAlternateErrorPagesEnabled;
@property(nonatomic) BOOL useSuggestEnabled;
@property(nonatomic) BOOL dnsPrefetchEnabled;
@property(nonatomic) BOOL safeBrowsingEnabled;
@property(nonatomic) BOOL metricsReportingEnabled;
@property(nonatomic) BOOL proxiesConfigureButtonEnabled;

// Designated initializer. |profile| should not be NULL.
- (id)initWithProfile:(Profile*)profile initialPage:(OptionsPage)initialPage;

// Show the preferences window.
- (void)showPreferences:(id)sender;

// Switch to the given preference page.
- (void)switchToPage:(OptionsPage)page animate:(BOOL)animate;

// Enables or disables the restoreOnStartup elements
- (void) setEnabledStateOfRestoreOnStartup;

// IBAction methods for responding to user actions.

// Basics panel
- (IBAction)addHomepage:(id)sender;
- (IBAction)removeSelectedHomepages:(id)sender;
- (IBAction)useCurrentPagesAsHomepage:(id)sender;
- (IBAction)manageSearchEngines:(id)sender;
- (IBAction)toggleInstant:(id)sender;
- (IBAction)learnMoreAboutInstant:(id)sender;
- (IBAction)makeDefaultBrowser:(id)sender;

// User Data panel
- (IBAction)doSyncAction:(id)sender;
- (IBAction)doSyncCustomize:(id)sender;
- (IBAction)doSyncReauthentication:(id)sender;
- (IBAction)showPrivacyDashboard:(id)sender;
- (IBAction)showSavedPasswords:(id)sender;
- (IBAction)showAutoFillSettings:(id)sender;
- (IBAction)importData:(id)sender;
- (IBAction)resetThemeToDefault:(id)sender;
- (IBAction)themesGallery:(id)sender;

// Under the hood
- (IBAction)showContentSettings:(id)sender;
- (IBAction)clearData:(id)sender;
- (IBAction)privacyLearnMore:(id)sender;
- (IBAction)browseDownloadLocation:(id)sender;
- (IBAction)resetAutoOpenFiles:(id)sender;
- (IBAction)changeFontAndLanguageSettings:(id)sender;
- (IBAction)openProxyPreferences:(id)sender;
- (IBAction)showCertificates:(id)sender;
- (IBAction)resetToDefaults:(id)sender;

// When a toolbar button is clicked
- (IBAction)toolbarButtonSelected:(id)sender;

@end

@interface PreferencesWindowController(Testing)

- (IntegerPrefMember*)lastSelectedPage;
- (NSToolbar*)toolbar;
- (NSView*)basicsView;
- (NSView*)personalStuffView;
- (NSView*)underTheHoodView;

// Converts the given OptionsPage value (which may be OPTIONS_PAGE_DEFAULT)
// into a concrete OptionsPage value.
- (OptionsPage)normalizePage:(OptionsPage)page;

// Returns the toolbar item corresponding to the given page.  Should be
// called only after awakeFromNib is.
- (NSToolbarItem*)getToolbarItemForPage:(OptionsPage)page;

// Returns the (normalized) page corresponding to the given toolbar item.
// Should be called only after awakeFromNib is.
- (OptionsPage)getPageForToolbarItem:(NSToolbarItem*)toolbarItem;

// Returns the view corresponding to the given page.  Should be called
// only after awakeFromNib is.
- (NSView*)getPrefsViewForPage:(OptionsPage)page;

@end
