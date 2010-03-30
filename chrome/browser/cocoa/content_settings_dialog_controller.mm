// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_settings_dialog_controller.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/content_exceptions_window_controller.h"
#import "chrome/browser/cocoa/cookies_window_controller.h"
#import "chrome/browser/geolocation/geolocation_content_settings_map.h"
#import "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"


namespace {

// Index of the "enabled" and "disabled" radio group settings in all tabs except
// for the cookies tab.
const NSInteger kEnabledIndex = 0;
const NSInteger kDisabledIndex = 1;

// Indices of the various cookie settings in the cookie radio group.
const NSInteger kCookieEnabledIndex = 0;
const NSInteger kCookieAskIndex = 1;
const NSInteger kCookieDisabledIndex = 2;

// Stores the currently visible content settings dialog, if any.
ContentSettingsDialogController* g_instance = nil;

// Indices of the various geolocation settings in the geolocation radio group.
const NSInteger kGeolocationEnabledIndex = 0;
const NSInteger kGeolocationAskIndex = 1;
const NSInteger kGeolocationDisabledIndex = 2;

}  // namespace


@interface ContentSettingsDialogController(Private)
- (id)initWithProfile:(Profile*)profile;
- (void)selectTab:(ContentSettingsType)settingsType;
- (void)showExceptionsForType:(ContentSettingsType)settingsType;

// Properties that the radio groups and checkboxes are bound to.
@property(assign, nonatomic) NSInteger cookieSettingIndex;
@property(assign, nonatomic) BOOL blockThirdPartyCookies;
@property(assign, nonatomic) BOOL clearSiteDataOnExit;
@property(assign, nonatomic) NSInteger imagesEnabledIndex;
@property(assign, nonatomic) NSInteger javaScriptEnabledIndex;
@property(assign, nonatomic) NSInteger popupsEnabledIndex;
@property(assign, nonatomic) NSInteger pluginsEnabledIndex;
@property(assign, nonatomic) NSInteger geolocationSettingIndex;

@end

namespace ContentSettingsDialogControllerInternal {

// A C++ class registered for changes in preferences.
class PrefObserverBridge : public NotificationObserver {
 public:
  PrefObserverBridge(ContentSettingsDialogController* controller)
      : controller_(controller) {}

  virtual ~PrefObserverBridge() {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED) {
      std::wstring* pref_name = Details<std::wstring>(details).ptr();
      if (*pref_name == prefs::kClearSiteDataOnExit) {
        // Update UI.
        [controller_ setClearSiteDataOnExit:[controller_ clearSiteDataOnExit]];
      }
    }
  }

 private:
  ContentSettingsDialogController* controller_;  // weak, owns us
};

}  // ContentSettingsDialogControllerInternal

@implementation ContentSettingsDialogController

+(id)showContentSettingsForType:(ContentSettingsType)settingsType
                        profile:(Profile*)profile {
  profile = profile->GetOriginalProfile();
  if (!g_instance)
    g_instance = [[self alloc] initWithProfile:profile];

  // The code doesn't expect multiple profiles. Check that support for that
  // hasn't been added.
  DCHECK(g_instance->profile_ == profile);

  // Select desired tab.
  if (settingsType == CONTENT_SETTINGS_TYPE_DEFAULT) {
    // Remember the last visited page from local state.
    int value = g_instance->lastSelectedTab_.GetValue();
    if (value >= 0 && value < CONTENT_SETTINGS_NUM_TYPES)
      settingsType = static_cast<ContentSettingsType>(value);
    if (settingsType == CONTENT_SETTINGS_TYPE_DEFAULT)
      settingsType = CONTENT_SETTINGS_TYPE_COOKIES;
  }
  // TODO(thakis): Autosave window pos.

  [g_instance selectTab:settingsType];
  [g_instance showWindow:nil];
  return g_instance;
}

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"ContentSettings"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;

    observer_.reset(
        new ContentSettingsDialogControllerInternal::PrefObserverBridge(self));
    clearSiteDataOnExit_.Init(prefs::kClearSiteDataOnExit,
                              profile->GetPrefs(), observer_.get());

    // We don't need to observe changes in this value.
    lastSelectedTab_.Init(prefs::kContentSettingsWindowLastTabIndex,
                          profile->GetPrefs(), NULL);
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
}

// NSWindowDelegate method.
- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
  g_instance = nil;
}

- (void)selectTab:(ContentSettingsType)settingsType {
  [self window];  // Make sure the nib file is loaded.
  DCHECK(tabView_);
  [tabView_ selectTabViewItemAtIndex:settingsType];
}

// NSTabViewDelegate method.
- (void)         tabView:(NSTabView*)tabView
    didSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  DCHECK_EQ(tabView_, tabView);
  NSInteger index = [tabView indexOfTabViewItem:tabViewItem];
  DCHECK_GT(index, CONTENT_SETTINGS_TYPE_DEFAULT);
  DCHECK_LT(index, CONTENT_SETTINGS_NUM_TYPES);
  if (index > CONTENT_SETTINGS_TYPE_DEFAULT &&
       index < CONTENT_SETTINGS_NUM_TYPES)
    lastSelectedTab_.SetValue(index);
}

// Let esc close the window.
- (void)cancel:(id)sender {
  [self close];
}

- (void)setCookieSettingIndex:(NSInteger)value {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  switch (value) {
    case kCookieEnabledIndex:  setting = CONTENT_SETTING_ALLOW; break;
    case kCookieAskIndex:      setting = CONTENT_SETTING_ASK;   break;
    case kCookieDisabledIndex: setting = CONTENT_SETTING_BLOCK; break;
    default:
      NOTREACHED();
  }
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES,
      setting);
}

- (NSInteger)cookieSettingIndex {
  switch (profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES)) {
    case CONTENT_SETTING_ALLOW: return kCookieEnabledIndex;
    case CONTENT_SETTING_ASK:   return kCookieAskIndex;
    case CONTENT_SETTING_BLOCK: return kCookieDisabledIndex;
    default:
      NOTREACHED();
      return kCookieEnabledIndex;
  }
}

- (BOOL)blockThirdPartyCookies {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  return settingsMap->BlockThirdPartyCookies();
}

- (void)setBlockThirdPartyCookies:(BOOL)value {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  settingsMap->SetBlockThirdPartyCookies(value);
}

- (BOOL)clearSiteDataOnExit {
  return clearSiteDataOnExit_.GetValue();
}

- (void)setClearSiteDataOnExit:(BOOL)value {
  return clearSiteDataOnExit_.SetValue(value);
}

// Shows the cookies controller.
- (IBAction)showCookies:(id)sender {
  // The cookie controller will autorelease itself when it's closed.
  BrowsingDataDatabaseHelper* databaseHelper =
      new BrowsingDataDatabaseHelper(profile_);
  BrowsingDataLocalStorageHelper* storageHelper =
      new BrowsingDataLocalStorageHelper(profile_);
  BrowsingDataAppCacheHelper* appcacheHelper =
      new BrowsingDataAppCacheHelper(profile_);
  CookiesWindowController* controller =
      [[CookiesWindowController alloc] initWithProfile:profile_
                                        databaseHelper:databaseHelper
                                         storageHelper:storageHelper
                                        appcacheHelper:appcacheHelper];
  [controller attachSheetTo:[self window]];
}

// Called when the user clicks the "Flash Player storage settings" button.
- (IBAction)openFlashPlayerSettings:(id)sender {
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

// Called when the user clicks the "Disable individual plug-ins..." button.
- (IBAction)openPluginsPage:(id)sender {
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(chrome::kChromeUIPluginsURL),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

- (IBAction)showCookieExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_COOKIES];
}

- (IBAction)showImagesExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_IMAGES];
}

- (IBAction)showJavaScriptExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_JAVASCRIPT];
}

- (IBAction)showPluginsExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_PLUGINS];
}

- (IBAction)showPopupsExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_POPUPS];
}

- (IBAction)showGeolocationExceptions:(id)sender {
  // TODO(thakis): Implement.
  NOTIMPLEMENTED();
}

- (void)showExceptionsForType:(ContentSettingsType)settingsType {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  [ContentExceptionsWindowController showForType:settingsType
                                     settingsMap:settingsMap];
}

- (void)setImagesEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, setting);
}

- (NSInteger)imagesEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kEnabledIndex : kDisabledIndex;
}

- (void)setJavaScriptEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, setting);
}

- (NSInteger)javaScriptEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kEnabledIndex : kDisabledIndex;
}

- (void)setPluginsEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, setting);
}

- (NSInteger)pluginsEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kEnabledIndex : kDisabledIndex;
}

- (void)setPopupsEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_POPUPS, setting);
}

- (NSInteger)popupsEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kEnabledIndex : kDisabledIndex;
}

- (void)setGeolocationSettingIndex:(NSInteger)value {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  switch (value) {
    case kGeolocationEnabledIndex:  setting = CONTENT_SETTING_ALLOW; break;
    case kGeolocationAskIndex:      setting = CONTENT_SETTING_ASK;   break;
    case kGeolocationDisabledIndex: setting = CONTENT_SETTING_BLOCK; break;
    default:
      NOTREACHED();
  }
  profile_->GetGeolocationContentSettingsMap()->SetDefaultContentSetting(
      setting);
}

- (NSInteger)geolocationSettingIndex {
  ContentSetting setting =
      profile_->GetGeolocationContentSettingsMap()->GetDefaultContentSetting();
  switch (setting) {
    case CONTENT_SETTING_ALLOW: return kGeolocationEnabledIndex;
    case CONTENT_SETTING_ASK:   return kGeolocationAskIndex;
    case CONTENT_SETTING_BLOCK: return kGeolocationDisabledIndex;
    default:
      NOTREACHED();
      return kGeolocationAskIndex;
  }
}

@end
