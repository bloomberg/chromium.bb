// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/content_settings_dialog_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#import "chrome/browser/content_settings/content_settings_details.h"
#import "chrome/browser/content_settings/host_content_settings_map.h"
#import "chrome/browser/geolocation/geolocation_content_settings_map.h"
#import "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#import "chrome/browser/notifications/desktop_notification_service.h"
#import "chrome/browser/notifications/notification_exceptions_table_model.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/content_settings/simple_content_exceptions_window_controller.h"
#import "chrome/browser/ui/cocoa/options/content_exceptions_window_controller.h"
#import "chrome/browser/ui/cocoa/options/cookies_window_controller.h"
#import "chrome/browser/ui/cocoa/tab_view_picker_table.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Stores the currently visible content settings dialog, if any.
ContentSettingsDialogController* g_instance = nil;

}  // namespace


@interface ContentSettingsDialogController(Private)
- (id)initWithProfile:(Profile*)profile;
- (void)selectTab:(ContentSettingsType)settingsType;
- (void)showExceptionsForType:(ContentSettingsType)settingsType;

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed.
- (void)prefChanged:(const std::string&)prefName;

// Callback when content settings are changed.
- (void)contentSettingsChanged:(ContentSettingsDetails*)details;

@end

namespace ContentSettingsDialogControllerInternal {

// A C++ class registered for changes in preferences.
class PrefObserverBridge : public NotificationObserver {
 public:
  PrefObserverBridge(ContentSettingsDialogController* controller)
      : controller_(controller), disabled_(false) {}

  virtual ~PrefObserverBridge() {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (disabled_)
      return;

    // This is currently used by most notifications.
    if (type == NotificationType::PREF_CHANGED) {
      std::string* detail = Details<std::string>(details).ptr();
      if (detail)
        [controller_ prefChanged:*detail];
    }

    // This is sent when the "is managed" state changes.
    // TODO(markusheintz): Move all content settings to this notification.
    if (type == NotificationType::CONTENT_SETTINGS_CHANGED) {
      ContentSettingsDetails* settings_details =
          Details<ContentSettingsDetails>(details).ptr();
      [controller_ contentSettingsChanged:settings_details];
    }
  }

  void SetDisabled(bool disabled) {
    disabled_ = disabled;
  }

 private:
  ContentSettingsDialogController* controller_;  // weak, owns us
  bool disabled_;  // true if notifications should be ignored.
};

// A C++ utility class to disable notifications for PrefsObserverBridge.
// The intended usage is to create this on the stack.
class PrefObserverDisabler {
 public:
  PrefObserverDisabler(PrefObserverBridge *bridge) : bridge_(bridge) {
    bridge_->SetDisabled(true);
  }

  ~PrefObserverDisabler() {
    bridge_->SetDisabled(false);
  }

 private:
  PrefObserverBridge *bridge_;
};

}  // ContentSettingsDialogControllerInternal

@implementation ContentSettingsDialogController

+ (id)showContentSettingsForType:(ContentSettingsType)settingsType
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
  [g_instance closeExceptionsSheet];
  return g_instance;
}

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"ContentSettings"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;

    observer_.reset(
        new ContentSettingsDialogControllerInternal::PrefObserverBridge(self));
    clearSiteDataOnExit_.Init(prefs::kClearSiteDataOnExit,
                              profile_->GetPrefs(), observer_.get());

    // Manually observe notifications for preferences that are grouped in
    // the HostContentSettingsMap or GeolocationContentSettingsMap.
    PrefService* prefs = profile_->GetPrefs();
    registrar_.Init(prefs);
    registrar_.Add(prefs::kBlockThirdPartyCookies, observer_.get());
    registrar_.Add(prefs::kBlockNonsandboxedPlugins, observer_.get());
    registrar_.Add(prefs::kDefaultContentSettings, observer_.get());
    registrar_.Add(prefs::kGeolocationDefaultContentSetting, observer_.get());

    // We don't need to observe changes in this value.
    lastSelectedTab_.Init(prefs::kContentSettingsWindowLastTabIndex,
                          profile_->GetPrefs(), NULL);
  }
  return self;
}

- (void)closeExceptionsSheet {
  NSWindow* attachedSheet = [[self window] attachedSheet];
  if (attachedSheet) {
    [NSApp endSheet:attachedSheet];
  }
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK(tabView_);
  DCHECK(tabViewPicker_);
  DCHECK_EQ(self, [[self window] delegate]);

  // Adapt views to potentially long localized strings.
  CGFloat windowDelta = 0;
  for (NSTabViewItem* tab in [tabView_ tabViewItems]) {
    NSArray* subviews = [[tab view] subviews];
    windowDelta = MAX(windowDelta,
                      cocoa_l10n_util::VerticallyReflowGroup(subviews));

    for (NSView* view in subviews) {
      // Since the tab pane is in a horizontal resizer in IB, it's convenient
      // to give all the subviews flexible width so that their sizes are
      // autoupdated in IB. However, in chrome, the subviews shouldn't have
      // flexible widths as this looks weird.
      [view setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
    }
  }

  NSString* label =
      l10n_util::GetNSStringWithFixup(IDS_CONTENT_SETTINGS_FEATURES_LABEL);
  label = [label stringByReplacingOccurrencesOfString:@":" withString:@""];
  [tabViewPicker_ setHeading:label];

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay)) {
    // The |pluginsEnabledIndex| property is bound to the selected *tag*,
    // so we don't have to worry about index shifts when removing a row
    // from the matrix.
    [pluginDefaultSettingMatrix_ removeRow:kPluginsAskIndex];
    NSArray* siblingViews = [[pluginDefaultSettingMatrix_ superview] subviews];
    for (NSView* view in siblingViews) {
      NSRect frame = [view frame];
      if (frame.origin.y < [pluginDefaultSettingMatrix_ frame].origin.y) {
        frame.origin.y +=
            ([pluginDefaultSettingMatrix_ cellSize].height +
             [pluginDefaultSettingMatrix_ intercellSpacing].height);
        [view setFrame:frame];
      }
    }
  }

  NSRect frame = [[self window] frame];
  frame.origin.y -= windowDelta;
  frame.size.height += windowDelta;
  [[self window] setFrame:frame display:NO];
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
    case kCookieDisabledIndex: setting = CONTENT_SETTING_BLOCK; break;
    default:
      NOTREACHED();
  }
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES,
      setting);
}

- (NSInteger)cookieSettingIndex {
  switch (profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES)) {
    case CONTENT_SETTING_ALLOW:        return kCookieEnabledIndex;
    case CONTENT_SETTING_BLOCK:        return kCookieDisabledIndex;
    default:
      NOTREACHED();
      return kCookieEnabledIndex;
  }
}

- (BOOL)cookieSettingsManaged {
  return profile_->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_COOKIES);
}

- (BOOL)blockThirdPartyCookies {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  return settingsMap->BlockThirdPartyCookies();
}

- (void)setBlockThirdPartyCookies:(BOOL)value {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  settingsMap->SetBlockThirdPartyCookies(value);
}

- (BOOL)blockThirdPartyCookiesManaged {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  return settingsMap->IsBlockThirdPartyCookiesManaged();
}

- (BOOL)clearSiteDataOnExitManaged {
  return clearSiteDataOnExit_.IsManaged();
}

- (BOOL)clearSiteDataOnExit {
  return clearSiteDataOnExit_.GetValue();
}

- (void)setClearSiteDataOnExit:(BOOL)value {
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  clearSiteDataOnExit_.SetValue(value);
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
  BrowsingDataIndexedDBHelper* indexedDBHelper =
      BrowsingDataIndexedDBHelper::Create(profile_);
  CookiesWindowController* controller =
      [[CookiesWindowController alloc] initWithProfile:profile_
                                        databaseHelper:databaseHelper
                                         storageHelper:storageHelper
                                        appcacheHelper:appcacheHelper
                                       indexedDBHelper:indexedDBHelper];
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableResourceContentSettings)) {
    HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
    HostContentSettingsMap* offTheRecordSettingsMap =
        profile_->HasOffTheRecordProfile() ?
            profile_->GetOffTheRecordProfile()->GetHostContentSettingsMap() :
            NULL;
    PluginExceptionsTableModel* model =
        new PluginExceptionsTableModel(settingsMap, offTheRecordSettingsMap);
    model->LoadSettings();
    [[SimpleContentExceptionsWindowController controllerWithTableModel:model]
        attachSheetTo:[self window]];
  } else {
    [self showExceptionsForType:CONTENT_SETTINGS_TYPE_PLUGINS];
  }
}

- (IBAction)showPopupsExceptions:(id)sender {
  [self showExceptionsForType:CONTENT_SETTINGS_TYPE_POPUPS];
}

- (IBAction)showGeolocationExceptions:(id)sender {
  GeolocationContentSettingsMap* settingsMap =
      profile_->GetGeolocationContentSettingsMap();
  GeolocationExceptionsTableModel* model =  // Freed by window controller.
      new GeolocationExceptionsTableModel(settingsMap);
  [[SimpleContentExceptionsWindowController controllerWithTableModel:model]
      attachSheetTo:[self window]];
}

- (IBAction)showNotificationsExceptions:(id)sender {
  DesktopNotificationService* service =
      profile_->GetDesktopNotificationService();
  NotificationExceptionsTableModel* model =  // Freed by window controller.
      new NotificationExceptionsTableModel(service);
  [[SimpleContentExceptionsWindowController controllerWithTableModel:model]
      attachSheetTo:[self window]];
}

- (void)showExceptionsForType:(ContentSettingsType)settingsType {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  HostContentSettingsMap* offTheRecordSettingsMap =
      profile_->HasOffTheRecordProfile() ?
          profile_->GetOffTheRecordProfile()->GetHostContentSettingsMap() :
          NULL;
  [[ContentExceptionsWindowController controllerForType:settingsType
                                            settingsMap:settingsMap
                                         otrSettingsMap:offTheRecordSettingsMap]
      attachSheetTo:[self window]];
}

- (void)setImagesEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kContentSettingsEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, setting);
}

- (NSInteger)imagesEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_IMAGES) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kContentSettingsEnabledIndex : kContentSettingsDisabledIndex;
}

- (BOOL)imagesSettingsManaged {
  return profile_->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_IMAGES);
}

- (void)setJavaScriptEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kContentSettingsEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, setting);
}

- (NSInteger)javaScriptEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kContentSettingsEnabledIndex : kContentSettingsDisabledIndex;
}

- (BOOL)javaScriptSettingsManaged {
  return profile_->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT);
}

- (void)setPluginsEnabledIndex:(NSInteger)value {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  switch (value) {
    case kPluginsAllowIndex:
      setting = CONTENT_SETTING_ALLOW;
      break;
    case kPluginsAskIndex:
      setting = CONTENT_SETTING_ASK;
      break;
    case kPluginsBlockIndex:
      setting = CONTENT_SETTING_BLOCK;
      break;
    default:
      NOTREACHED();
  }
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, setting);
}

- (NSInteger)pluginsEnabledIndex {
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
  ContentSetting setting =
      map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return kPluginsAllowIndex;
    case CONTENT_SETTING_ASK:
      if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay))
        return kPluginsAskIndex;
      // Fall through to the next case.
    case CONTENT_SETTING_BLOCK:
      return kPluginsBlockIndex;
    default:
      NOTREACHED();
      return kPluginsAllowIndex;
  }
}

- (BOOL)pluginsSettingsManaged {
  return profile_->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_PLUGINS);
}

- (void)setPopupsEnabledIndex:(NSInteger)value {
  ContentSetting setting = value == kContentSettingsEnabledIndex ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_POPUPS, setting);
}

- (NSInteger)popupsEnabledIndex {
  HostContentSettingsMap* settingsMap = profile_->GetHostContentSettingsMap();
  bool enabled =
      settingsMap->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS) ==
      CONTENT_SETTING_ALLOW;
  return enabled ? kContentSettingsEnabledIndex : kContentSettingsDisabledIndex;
}

- (BOOL)popupsSettingsManaged {
  return profile_->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_POPUPS);
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
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
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

- (void)setNotificationsSettingIndex:(NSInteger)value {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  switch (value) {
    case kNotificationsEnabledIndex:  setting = CONTENT_SETTING_ALLOW; break;
    case kNotificationsAskIndex:      setting = CONTENT_SETTING_ASK;   break;
    case kNotificationsDisabledIndex: setting = CONTENT_SETTING_BLOCK; break;
    default:
      NOTREACHED();
  }
  ContentSettingsDialogControllerInternal::PrefObserverDisabler
      disabler(observer_.get());
  profile_->GetDesktopNotificationService()->SetDefaultContentSetting(
      setting);
}

- (NSInteger)notificationsSettingIndex {
  ContentSetting setting =
      profile_->GetDesktopNotificationService()->GetDefaultContentSetting();
  switch (setting) {
    case CONTENT_SETTING_ALLOW: return kNotificationsEnabledIndex;
    case CONTENT_SETTING_ASK:   return kNotificationsAskIndex;
    case CONTENT_SETTING_BLOCK: return kNotificationsDisabledIndex;
    default:
      NOTREACHED();
      return kGeolocationAskIndex;
  }
}

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed and should not be NULL.
- (void)prefChanged:(const std::string&)prefName {
  if (prefName == prefs::kClearSiteDataOnExit) {
    [self willChangeValueForKey:@"clearSiteDataOnExit"];
    [self didChangeValueForKey:@"clearSiteDataOnExit"];
    [self willChangeValueForKey:@"clearSiteDataOnExitManaged"];
    [self didChangeValueForKey:@"clearSiteDataOnExitManaged"];
  }
  if (prefName == prefs::kBlockThirdPartyCookies) {
    [self willChangeValueForKey:@"blockThirdPartyCookies"];
    [self didChangeValueForKey:@"blockThirdPartyCookies"];
    [self willChangeValueForKey:@"blockThirdPartyCookiesManaged"];
    [self didChangeValueForKey:@"blockThirdPartyCookiesManaged"];
  }
  if (prefName == prefs::kBlockNonsandboxedPlugins) {
    [self willChangeValueForKey:@"pluginsEnabledIndex"];
    [self didChangeValueForKey:@"pluginsEnabledIndex"];
  }
  if (prefName == prefs::kDefaultContentSettings) {
    // We don't know exactly which setting has changed, so we'll tickle all
    // of the properties that apply to kDefaultContentSettings.  This will
    // keep the UI up-to-date.
    [self willChangeValueForKey:@"cookieSettingIndex"];
    [self didChangeValueForKey:@"cookieSettingIndex"];
    [self willChangeValueForKey:@"imagesEnabledIndex"];
    [self didChangeValueForKey:@"imagesEnabledIndex"];
    [self willChangeValueForKey:@"javaScriptEnabledIndex"];
    [self didChangeValueForKey:@"javaScriptEnabledIndex"];
    [self willChangeValueForKey:@"pluginsEnabledIndex"];
    [self didChangeValueForKey:@"pluginsEnabledIndex"];
    [self willChangeValueForKey:@"popupsEnabledIndex"];
    [self didChangeValueForKey:@"popupsEnabledIndex"];

    // Updates the "Enable" state of the radio groups and the exception buttons.
    [self willChangeValueForKey:@"cookieSettingsManaged"];
    [self didChangeValueForKey:@"cookieSettingsManaged"];
    [self willChangeValueForKey:@"imagesSettingsManaged"];
    [self didChangeValueForKey:@"imagesSettingsManaged"];
    [self willChangeValueForKey:@"javaScriptSettingsManaged"];
    [self didChangeValueForKey:@"javaScriptSettingsManaged"];
    [self willChangeValueForKey:@"pluginsSettingsManaged"];
    [self didChangeValueForKey:@"pluginsSettingsManaged"];
    [self willChangeValueForKey:@"popupsSettingsManaged"];
    [self didChangeValueForKey:@"popupsSettingsManaged"];
  }
  if (prefName == prefs::kGeolocationDefaultContentSetting) {
    [self willChangeValueForKey:@"geolocationSettingIndex"];
    [self didChangeValueForKey:@"geolocationSettingIndex"];
  }
  if (prefName == prefs::kDesktopNotificationDefaultContentSetting) {
    [self willChangeValueForKey:@"notificationsSettingIndex"];
    [self didChangeValueForKey:@"notificationsSettingIndex"];
  }
}

- (void)contentSettingsChanged:(ContentSettingsDetails*)details {
  [self prefChanged:prefs::kBlockNonsandboxedPlugins];
  [self prefChanged:prefs::kDefaultContentSettings];
}

@end
