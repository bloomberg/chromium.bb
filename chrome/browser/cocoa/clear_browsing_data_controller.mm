// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/clear_browsing_data_controller.h"

#include "base/mac_util.h"
#include "base/scoped_nsobject.h"
#include "base/singleton.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/profile.h"

NSString* const kClearBrowsingDataControllerDidDelete =
    @"kClearBrowsingDataControllerDidDelete";
NSString* const kClearBrowsingDataControllerRemoveMask =
    @"kClearBrowsingDataControllerRemoveMask";

@interface ClearBrowsingDataController(Private)
- (void)initFromPrefs;
- (void)persistToPrefs;
- (void)dataRemoverDidFinish;
@end

class ClearBrowsingObserver : public BrowsingDataRemover::Observer {
 public:
  ClearBrowsingObserver(ClearBrowsingDataController* controller)
      : controller_(controller) { }
  void OnBrowsingDataRemoverDone() { [controller_ dataRemoverDidFinish]; }
 private:
  ClearBrowsingDataController* controller_;
};

namespace {

typedef std::map<Profile*, ClearBrowsingDataController*> ProfileControllerMap;

} // namespace

@implementation ClearBrowsingDataController

@synthesize clearBrowsingHistory = clearBrowsingHistory_;
@synthesize clearDownloadHistory = clearDownloadHistory_;
@synthesize emptyCache = emptyCache_;
@synthesize deleteCookies = deleteCookies_;
@synthesize clearSavedPasswords = clearSavedPasswords_;
@synthesize clearFormData = clearFormData_;
@synthesize timePeriod = timePeriod_;
@synthesize isClearing = isClearing_;

+ (void)showClearBrowsingDialogForProfile:(Profile*)profile {
  ClearBrowsingDataController* controller =
      [ClearBrowsingDataController controllerForProfile:profile];
  if (![controller isWindowLoaded]) {
    [controller runModalDialog];
  }
}

+ (ClearBrowsingDataController *)controllerForProfile:(Profile*)profile {
  // Get the original profile in case we get here from an incognito window
  // |GetOriginalProfile()| will return the same profile if it is the original
  // profile.
  profile = profile->GetOriginalProfile();

  ProfileControllerMap* map = Singleton<ProfileControllerMap>::get();
  DCHECK(map != NULL);
  ProfileControllerMap::iterator it = map->find(profile);
  if (it == map->end()) {
    // Since we don't currently support multiple profiles, this class
    // has not been tested against this case.
    if (map->size() != 0) {
      return nil;
    }

    ClearBrowsingDataController* controller =
        [[self alloc] initWithProfile:profile];
    it = map->insert(std::make_pair(profile, controller)).first;
  }
  return it->second;
}

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString *nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"ClearBrowsingData"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    observer_.reset(new ClearBrowsingObserver(self));
    [self initFromPrefs];
  }
  return self;
}

- (void)dealloc {
  if (remover_) {
    // We were destroyed while clearing history was in progress. This can only
    // occur during automated tests (normally the user can't close the dialog
    // while clearing is in progress as the dialog is modal and not closeable).
    remover_->RemoveObserver(observer_.get());
  }

  [super dealloc];
}

// Run application modal.
- (void)runModalDialog {
  [NSApp runModalForWindow:[self window]];
}

- (int)removeMask {
  int removeMask = 0L;
  if (clearBrowsingHistory_)
    removeMask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (clearDownloadHistory_)
    removeMask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (emptyCache_)
    removeMask |= BrowsingDataRemover::REMOVE_CACHE;
  if (deleteCookies_)
     removeMask |= BrowsingDataRemover::REMOVE_COOKIES;
  if (clearSavedPasswords_)
     removeMask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (clearFormData_)
    removeMask |= BrowsingDataRemover::REMOVE_FORM_DATA;
  return removeMask;
}

// Called when the user clicks the "clear" button. Do the work and persist
// the prefs for next time. We don't stop the modal session until we get
// the callback from the BrowsingDataRemover so the window stays on the screen.
// While we're working, dim the buttons so the user can't click them.
- (IBAction)clearData:(id)sender {
  // Set that we're working so that the buttons disable.
  [self setIsClearing:YES];

  [self persistToPrefs];

  // BrowsingDataRemover deletes itself when done.
  remover_ = new BrowsingDataRemover(profile_,
      static_cast<BrowsingDataRemover::TimePeriod>(timePeriod_),
      base::Time());
  remover_->AddObserver(observer_.get());
  remover_->Remove([self removeMask]);
}

// Called when the user clicks the cancel button. All we need to do is stop
// the modal session.
- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

- (void)closeDialog {
  ProfileControllerMap* map = Singleton<ProfileControllerMap>::get();
  ProfileControllerMap::iterator it = map->find(profile_);
  if (it != map->end()) {
    map->erase(it);
  }
  [self autorelease];
  [[self window] orderOut:self];
  [NSApp stopModal];
}

// Initialize the bools from prefs using the setters to be KVO-compliant.
- (void)initFromPrefs {
  PrefService* prefs = profile_->GetPrefs();
  [self setClearBrowsingHistory:
      prefs->GetBoolean(prefs::kDeleteBrowsingHistory)];
  [self setClearDownloadHistory:
      prefs->GetBoolean(prefs::kDeleteDownloadHistory)];
  [self setEmptyCache:prefs->GetBoolean(prefs::kDeleteCache)];
  [self setDeleteCookies:prefs->GetBoolean(prefs::kDeleteCookies)];
  [self setClearSavedPasswords:prefs->GetBoolean(prefs::kDeletePasswords)];
  [self setClearFormData:prefs->GetBoolean(prefs::kDeleteFormData)];
  [self setTimePeriod:prefs->GetInteger(prefs::kDeleteTimePeriod)];
}

// Save the checkbox values to the preferences.
- (void)persistToPrefs {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kDeleteBrowsingHistory,
                    [self clearBrowsingHistory]);
  prefs->SetBoolean(prefs::kDeleteDownloadHistory,
                    [self clearDownloadHistory]);
  prefs->SetBoolean(prefs::kDeleteCache, [self emptyCache]);
  prefs->SetBoolean(prefs::kDeleteCookies, [self deleteCookies]);
  prefs->SetBoolean(prefs::kDeletePasswords, [self clearSavedPasswords]);
  prefs->SetBoolean(prefs::kDeleteFormData, [self clearFormData]);
  prefs->SetInteger(prefs::kDeleteTimePeriod, [self timePeriod]);
}

// Called when the data remover object is done with its work. Close the window.
// The remover will delete itself. End the modal session at this point.
- (void)dataRemoverDidFinish {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  int removeMask = [self removeMask];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:removeMask]
                                forKey:kClearBrowsingDataControllerRemoveMask];
  [center postNotificationName:kClearBrowsingDataControllerDidDelete
                        object:self
                      userInfo:userInfo];

  [self closeDialog];
  [[self window] orderOut:self];
  [self setIsClearing:NO];
  remover_ = NULL;
}

@end
