// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/clear_browsing_data_controller.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/mac_util.h"
#include "base/scoped_nsobject.h"
#include "base/singleton.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

NSString* const kClearBrowsingDataControllerDidDelete =
    @"kClearBrowsingDataControllerDidDelete";
NSString* const kClearBrowsingDataControllerRemoveMask =
    @"kClearBrowsingDataControllerRemoveMask";

namespace {

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order top down.
NSInteger CompareFrameY(id view1, id view2, void* context) {
  CGFloat y1 = NSMinY([view1 frame]);
  CGFloat y2 = NSMinY([view2 frame]);
  if (y1 < y2)
    return NSOrderedDescending;
  else if (y1 > y2)
    return NSOrderedAscending;
  else
    return NSOrderedSame;
}

typedef std::map<Profile*, ClearBrowsingDataController*> ProfileControllerMap;

}  // namespace

@interface ClearBrowsingDataController(Private)
- (void)initFromPrefs;
- (void)persistToPrefs;
- (void)dataRemoverDidFinish;
- (void)syncStateChanged;
@end

class ClearBrowsingObserver : public BrowsingDataRemover::Observer,
                              public ProfileSyncServiceObserver {
 public:
  ClearBrowsingObserver(ClearBrowsingDataController* controller)
      : controller_(controller) { }
  void OnBrowsingDataRemoverDone() { [controller_ dataRemoverDidFinish]; }
  void OnStateChanged() { [controller_ syncStateChanged]; }
 private:
  ClearBrowsingDataController* controller_;
};

@implementation ClearBrowsingDataController

@synthesize clearBrowsingHistory = clearBrowsingHistory_;
@synthesize clearDownloadHistory = clearDownloadHistory_;
@synthesize emptyCache = emptyCache_;
@synthesize deleteCookies = deleteCookies_;
@synthesize clearSavedPasswords = clearSavedPasswords_;
@synthesize clearFormData = clearFormData_;
@synthesize timePeriod = timePeriod_;
@synthesize isClearing = isClearing_;
@synthesize clearingStatus = clearingStatus_;

+ (void)showClearBrowsingDialogForProfile:(Profile*)profile {
  ClearBrowsingDataController* controller =
      [ClearBrowsingDataController controllerForProfile:profile];
  if (![controller isWindowLoaded]) {
    // This function needs to return instead of blocking, to match the Windows
    // version. It caused problems when launching the dialog from the
    // DomUI history page. See bug and code review for more details.
    // http://crbug.com/37976
    [controller performSelector:@selector(runModalDialog)
                     withObject:nil
                     afterDelay:0];
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
    observer_.reset(new ClearBrowsingObserver(self));

    // Always show preferences for the original profile. Most state when off
    // the record comes from the original profile, but we explicitly use
    // the original profile to avoid potential problems.
    profile_ = profile->GetOriginalProfile();
    sync_service_ = profile_->GetProfileSyncService();

    if (sync_service_) {
      sync_service_->ResetClearServerDataState();
      sync_service_->AddObserver(observer_.get());
    }

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
  if (sync_service_)
    sync_service_->RemoveObserver(observer_.get());
  [self setClearingStatus:nil];

  [super dealloc];
}

// Run application modal.
- (void)runModalDialog {
  // Check again to make sure there is only one window.  Since we use
  // |performSelector:afterDelay:| it is possible for this to somehow be
  // triggered twice.
  DCHECK([NSThread isMainThread]);
  if (![self isWindowLoaded]) {
    // It takes two passes to adjust the window size. The first pass is to
    // determine the width of all the non-wrappable items. The window is then
    // resized to that width. Once the width is fixed, the heights of the
    // variable-height items can then be calculated, the items can be spaced,
    // and the window resized (again) if necessary.

    NSWindow* window = [self window];

    // Adjust the widths of non-wrappable items.
    CGFloat maxWidthGrowth = 0.0;
    Class widthBasedTweakerClass = [GTMWidthBasedTweaker class];
    for (NSTabViewItem* tabViewItem in [tabView_ tabViewItems])
      for (NSView* subView in [[tabViewItem view] subviews])
        if ([subView isKindOfClass:widthBasedTweakerClass]) {
          GTMWidthBasedTweaker* tweaker = (GTMWidthBasedTweaker*)subView;
          CGFloat delta = [tweaker changedWidth];
          maxWidthGrowth = std::max(maxWidthGrowth, delta);
        }

    // Adjust the width of the window.
    if (maxWidthGrowth > 0.0) {
      NSSize adjustSize = NSMakeSize(maxWidthGrowth, 0);
      adjustSize = [[window contentView] convertSize:adjustSize toView:nil];
      NSRect windowFrame = [window frame];
      windowFrame.size.width += adjustSize.width;
      [window setFrame:windowFrame display:NO];
    }

    // Adjust the heights and locations of the items on the "Other data" tab.
    CGFloat cumulativeHeightGrowth = 0.0;
    NSArray* subViews =
        [[otherDataTab_ subviews] sortedArrayUsingFunction:CompareFrameY
                                                   context:NULL];
    for (NSView* view in subViews) {
      if ([view isHidden])
        continue;

      if ([objectsToVerticallySize_ containsObject:view]) {
        DCHECK([view isKindOfClass:[NSTextField class]]);
        CGFloat viewHeightGrowth = [GTMUILocalizerAndLayoutTweaker
            sizeToFitFixedWidthTextField:(NSTextField*)view];
        if (viewHeightGrowth > 0.0)
          cumulativeHeightGrowth += viewHeightGrowth;
      }

      NSRect viewFrame = [view frame];
      viewFrame.origin.y -= cumulativeHeightGrowth;
      [view setFrame:viewFrame];
    }

    // Adjust the height of the window.
    if (cumulativeHeightGrowth > 0.0) {
      NSSize adjustSize = NSMakeSize(0, cumulativeHeightGrowth);
      adjustSize = [[window contentView] convertSize:adjustSize toView:nil];
      NSRect windowFrame = [window frame];
      windowFrame.size.height += adjustSize.height;
      [window setFrame:windowFrame display:NO];
    }

    // Now start the modal loop.
    [NSApp runModalForWindow:window];
  }
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
  [self setClearingStatus:l10n_util::GetNSString(IDS_CLEAR_DATA_DELETING)];
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

// Called when the user clicks the "Flash Player storage settings" button.
- (IBAction)openFlashPlayerSettings:(id)sender {
  // The "Clear Data" dialog is app-modal on OS X. Hence, close it before
  // opening a tab with flash settings.
  [self closeDialog];

  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

- (IBAction)openGoogleDashboard:(id)sender {
  // The "Clear Data" dialog is app-modal on OS X. Hence, close it before
  // opening a tab with the dashboard.
  [self closeDialog];

  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_PRIVACY_DASHBOARD_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
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
  [self setClearingStatus:nil];
  [self setIsClearing:NO];
  remover_ = NULL;
}

- (IBAction)stopSyncAndDeleteData:(id)sender {
  // Protect against the unlikely case where the server received a message, and
  // the syncer syncs and resets itself before the user tries pressing the Clear
  // button in this dialog again. TODO(raz) Confirm whether we have an issue
  // here
  if (sync_service_->HasSyncSetupCompleted()) {
    bool clear = platform_util::SimpleYesNoBox(
        nil,
        l10n_util::GetStringUTF16(IDS_CONFIRM_CLEAR_TITLE),
        l10n_util::GetStringUTF16(IDS_CONFIRM_CLEAR_DESCRIPTION));
    if (clear) {
      sync_service_->ClearServerData();
      [self syncStateChanged];
    }
  }
}

- (void)syncStateChanged {
  bool deleteInProgress = false;

  ProfileSyncService::ClearServerDataState clearState =
      sync_service_->GetClearServerDataState();
  sync_service_->ResetClearServerDataState();

  switch (clearState) {
    case ProfileSyncService::CLEAR_NOT_STARTED:
      // This can occur on a first start and after a failed clear (which does
      // not close the tab). Do nothing.
      break;
    case ProfileSyncService::CLEAR_CLEARING:
      // Clearing buttons on all tabs are disabled at this point, throbber is
      // going.
      [self setClearingStatus:l10n_util::GetNSString(IDS_CLEAR_DATA_SENDING)];
      deleteInProgress = true;
      break;
    case ProfileSyncService::CLEAR_FAILED:
      // Show an error and reallow clearing.
      [self setClearingStatus:l10n_util::GetNSString(IDS_CLEAR_DATA_ERROR)];
      deleteInProgress = false;
      break;
    case ProfileSyncService::CLEAR_SUCCEEDED:
      // Close the dialog box, success!
      [self setClearingStatus:nil];
      deleteInProgress = false;
      [self closeDialog];
      break;
  }

  [self setIsClearing:deleteInProgress];
}

- (BOOL)isSyncVisible {
  return YES;
}

- (BOOL)isSyncEnabled {
  return sync_service_ && sync_service_->HasSyncSetupCompleted();
}

- (NSFont*)labelFont {
  return [NSFont boldSystemFontOfSize:13];
}

@end
