// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/import_settings_dialog.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/profile.h"

namespace {

bool importSettingsDialogVisible = false;

}  // namespace

@interface ImportSettingsDialogController ()

@property(assign, readwrite, nonatomic) BOOL historyAvailable;
@property(assign, readwrite, nonatomic) BOOL favoritesAvailable;
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

@interface ImportSettingsDialogController (Private)

// Initialize the dialog controller with either the default profile or
// the profile for the current browser.
- (id)initWithProfile:(Profile*)profile;

// Present the app modal dialog.
- (void)runModalDialog;

// Close the modal dialog.
- (void)closeDialog;

@end

@implementation ImportSettingsDialogController

@synthesize sourceBrowserIndex = sourceBrowserIndex_;
@synthesize importHistory = importHistory_;
@synthesize importFavorites = importFavorites_;
@synthesize importPasswords = importPasswords_;
@synthesize importSearchEngines = importSearchEngines_;
@synthesize historyAvailable = historyAvailable_;
@synthesize favoritesAvailable = favoritesAvailable_;
@synthesize passwordsAvailable = passwordsAvailable_;
@synthesize searchEnginesAvailable = searchEnginesAvailable_;

// Set bindings dependencies for importSomething property.
+ (NSSet*)keyPathsForValuesAffectingImportSomething {
  return [NSSet setWithObjects:@"importHistory", @"importFavorites",
          @"importPasswords", @"importSearchEngines", nil];
}

+ (void)showImportSettingsDialogForProfile:(Profile*)profile {
  // Don't display if already visible.
  if (importSettingsDialogVisible)
    return;
  ImportSettingsDialogController* controller =
      [[ImportSettingsDialogController alloc] initWithProfile:profile];
  [controller runModalDialog];
}

- (id)initWithProfile:(Profile*)profile {
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
    const importer::ProfileInfo& sourceProfile =
        importerList.GetSourceProfileInfoAt(i);
    NSString* browserName =
        base::SysWideToNSString(sourceProfile.description);
    uint16 browserServices = sourceProfile.services_supported;
    ImportSettingsProfile* settingsProfile =
        [ImportSettingsProfile
         importSettingsProfileWithBrowserName:browserName
                                     services:browserServices];
    [browserProfiles addObject:settingsProfile];
  }
  if ((self = [self initWithProfiles:browserProfiles])) {
    profile_ = profile;
  }
  return self;
}

- (id)initWithProfiles:(NSArray*)profiles {
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
  return [self initWithProfile:NULL];
}

- (void)awakeFromNib {
  // Force an update of the checkbox enabled states.
  [self setSourceBrowserIndex:0];
}

// Run application modal.
- (void)runModalDialog {
  importSettingsDialogVisible = true;
  [NSApp runModalForWindow:[self window]];
}

- (IBAction)ok:(id)sender {
  [self closeDialog];
  const importer::ProfileInfo& sourceProfile =
      importerList_.get()->GetSourceProfileInfoAt([self sourceBrowserIndex]);
  uint16 items = sourceProfile.services_supported;
  uint16 servicesToImport = items & [self servicesToImport];
  if (servicesToImport) {
    if (profile_) {
      ImporterHost* importerHost = new ExternalProcessImporterHost;
      // Note that a side effect of the following call is to cause the
      // importerHost to be disposed once the import has completed.
      StartImportingWithUI(nil, servicesToImport, importerHost,
                           sourceProfile, profile_, nil, false);
    }
  } else {
    LOG(WARNING) << "There were no settings to import from '"
                 << sourceProfile.description << "'.";
  }
}

- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

- (void)closeDialog {
  importSettingsDialogVisible = false;
  [[self window] orderOut:self];
  [NSApp stopModal];
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
  [self setHistoryAvailable:(items & importer::HISTORY) ? YES : NO];
  [self setImportHistory:[self historyAvailable]];
  [self setFavoritesAvailable:(items & importer::FAVORITES) ? YES : NO];
  [self setImportFavorites:[self favoritesAvailable]];
  [self setPasswordsAvailable:(items & importer::PASSWORDS) ? YES : NO];
  [self setImportPasswords:[self passwordsAvailable]];
  [self setSearchEnginesAvailable:(items & importer::SEARCH_ENGINES) ?
      YES : NO];
  [self setImportSearchEngines:[self searchEnginesAvailable]];
}

- (uint16)servicesToImport {
  uint16 servicesToImport = 0;
  if ([self importHistory]) servicesToImport |= importer::HISTORY;
  if ([self importFavorites]) servicesToImport |= importer::FAVORITES;
  if ([self importPasswords]) servicesToImport |= importer::PASSWORDS;
  if ([self importSearchEngines]) servicesToImport |=
      importer::SEARCH_ENGINES;
  return servicesToImport;
}

// KVO accessor which returns YES if at least one of the services
// provided by the selected profile has been marked for importing
// and bound to the OK button's enable property.
- (BOOL)importSomething {
  return [self importHistory] || [self importFavorites] ||
      [self importPasswords] || [self importSearchEngines];
}

@end
