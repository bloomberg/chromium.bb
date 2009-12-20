// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/import_settings_dialog.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/profile.h"

@interface ImportSettingsDialogController ()

@property(assign, readwrite, nonatomic) BOOL historyAvailable;
@property(assign, readwrite, nonatomic) BOOL favoritesAvailable;
@property(assign, readwrite, nonatomic) BOOL cookiesAvailable;
@property(assign, readwrite, nonatomic) BOOL passwordsAvailable;
@property(assign, readwrite, nonatomic) BOOL searchEnginesAvailable;

@end

@implementation ImportSettingsProfile

@synthesize browserName = browserName_;
@synthesize services = services_;

+ (id)importSettingsProfileWithBrowserName:(NSString*)browserName
                                  services:(uint16)services {
  id settingsProfile = [[[ImportSettingsProfile alloc]
                        initWithBrowserName:browserName
                         services:services] autorelease];
  return settingsProfile;
}

- (id)initWithBrowserName:(NSString*)browserName
                 services:(uint16)services {
  DCHECK(browserName && services);
  if ((self = [super init])) {
    if (browserName && services != 0) {
      browserName_ = [browserName retain];
      services_ = services;
    } else {
      [self release];
      self = nil;
    }
  }
  return self;
}

- (id)init {
  NOTREACHED();  // Should never be called.
  return [self initWithBrowserName:NULL services:0];
}

- (void)dealloc {
  [browserName_ release];
  [super dealloc];
}

@end

@implementation ImportSettingsDialogController

@synthesize sourceBrowserIndex = sourceBrowserIndex_;
@synthesize importHistory = importHistory_;
@synthesize importFavorites = importFavorites_;
@synthesize importCookies = importCookies_;
@synthesize importPasswords = importPasswords_;
@synthesize importSearchEngines = importSearchEngines_;
@synthesize historyAvailable = historyAvailable_;
@synthesize favoritesAvailable = favoritesAvailable_;
@synthesize cookiesAvailable = cookiesAvailable_;
@synthesize passwordsAvailable = passwordsAvailable_;
@synthesize searchEnginesAvailable = searchEnginesAvailable_;

// Set bindings dependencies for importSomething property.
+ (NSSet*)keyPathsForValuesAffectingImportSomething {
  return [NSSet setWithObjects:@"importHistory", @"importFavorites",
          @"importCookies", @"importPasswords", @"importSearchEngines", nil];
}

- (id)initWithProfile:(Profile*)profile
         parentWindow:(NSWindow*)parentWindow {
  // Collect profile information (profile name and the services which can
  // be imported from each) into an array of ImportSettingsProfile which
  // are bound to the Browser List array controller and the popup name
  // presentation.  The services element is used to indirectly control
  // checkbox enabling.
  importerList_.reset(new ImporterList);
  ImporterList& importerList(*(importerList_.get()));
  importerList.DetectSourceProfiles();
  int profilesCount = importerList.GetAvailableProfileCount();
  // There shoule be at least the default profile so this should never be zero.
  DCHECK(profilesCount > 0);
  NSMutableArray* browserProfiles =
      [NSMutableArray arrayWithCapacity:profilesCount];
  for (int i = 0; i < profilesCount; ++i) {
    const ProfileInfo& sourceProfile = importerList.GetSourceProfileInfoAt(i);
    NSString* browserName =
        base::SysWideToNSString(sourceProfile.description);
    uint16 browserServices = sourceProfile.services_supported;
    ImportSettingsProfile* settingsProfile =
        [ImportSettingsProfile
         importSettingsProfileWithBrowserName:browserName
                                     services:browserServices];
    [browserProfiles addObject:settingsProfile];
  }
  if ((self = [self initWithProfiles:browserProfiles
                        parentWindow:parentWindow])) {
    profile_ = profile;
    parentWindow_ = parentWindow;
  }
  return self;
}

- (id)initWithProfiles:(NSArray*)profiles
          parentWindow:(NSWindow*)parentWindow {
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"ImportSettingsDialog"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    sourceBrowsersList_.reset([profiles retain]);
    // Create and initialize an importerList_ when running unit tests.
    if (!importerList_.get()) {
      importerList_.reset(new ImporterList);
      ImporterList& importerList(*(importerList_.get()));
      importerList.DetectSourceProfiles();
    }
  }
  return self;
}

- (id)init {
  return [self initWithProfile:NULL parentWindow:nil];
}

- (void)awakeFromNib {
  // Force an update of the checkbox enabled states.
  [self setSourceBrowserIndex:0];
}

- (void)runModalDialog {
  // If there is no parentWindow_ then this will present as a regular
  // modal dialog not hanging off of any other window.
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (IBAction)ok:(id)sender {
  const ProfileInfo& sourceProfile =
      importerList_.get()->GetSourceProfileInfoAt([self sourceBrowserIndex]);
  uint16 items = sourceProfile.services_supported;
  // ProfileInfo.services_supported is a uint16 while the call to
  // StartImportingWithUI requires an int16.
  int16 servicesToImport = static_cast<int16>(items & [self servicesToImport]);
  if (servicesToImport) {
    if (profile_) {
      ImporterHost* importerHost = new ImporterHost;
      // Note that a side effect of the following call is to cause the
      // importerHost to be disposed once the import has completed.
      StartImportingWithUI(nil, servicesToImport, importerHost,
                           sourceProfile, profile_, nil, false);
    }
  } else {
    LOG(WARNING) << "There were no settings to import from '"
                 << sourceProfile.description << "'.";
  }
  [NSApp endSheet:[self window]];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [sheet close];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

#pragma mark Accessors

- (NSArray*)sourceBrowsersList {
  return sourceBrowsersList_.get();
}

// Accessor which cascades selected-browser changes into a re-evaluation of the
// available services and the associated checkbox enable and checked states.
- (void)setSourceBrowserIndex:(NSUInteger)browserIndex {
  sourceBrowserIndex_ = browserIndex;
  ImportSettingsProfile* profile =
      [sourceBrowsersList_.get() objectAtIndex:browserIndex];
  uint16 items = [profile services];
  [self setHistoryAvailable:(items & HISTORY) ? YES : NO];
  [self setImportHistory:[self historyAvailable]];
  [self setFavoritesAvailable:(items & FAVORITES) ? YES : NO];
  [self setImportFavorites:[self favoritesAvailable]];
  [self setCookiesAvailable:(items & COOKIES) ? YES : NO];
  [self setImportCookies:[self cookiesAvailable]];
  [self setPasswordsAvailable:(items & PASSWORDS) ? YES : NO];
  [self setImportPasswords:[self passwordsAvailable]];
  [self setSearchEnginesAvailable:(items & SEARCH_ENGINES) ? YES : NO];
  [self setImportSearchEngines:[self searchEnginesAvailable]];
}

- (uint16)servicesToImport {
  uint16 servicesToImport = 0;
  if ([self importHistory]) servicesToImport |= HISTORY;
  if ([self importFavorites]) servicesToImport |= FAVORITES;
  if ([self importCookies]) servicesToImport |= COOKIES;
  if ([self importPasswords]) servicesToImport |= PASSWORDS;
  if ([self importSearchEngines]) servicesToImport |= SEARCH_ENGINES;
  return servicesToImport;
}

// KVO accessor which returns YES if at least one of the services
// provided by the selected profile has been marked for importing
// and bound to the OK button's enable property.
- (BOOL)importSomething {
  return [self importHistory] || [self importFavorites] ||
      [self importCookies] || [self importPasswords] ||
      [self importSearchEngines];
}

@end
