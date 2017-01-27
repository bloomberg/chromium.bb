// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sync/sync_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#include "ios/chrome/browser/ui/sync/sync_error_infobar_delegate.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Enumerated constants for logging when a sign-in error infobar was shown
// to the user. This was added for crbug/265352 to quantify how often this
// bug shows up in the wild. The logged histogram count should be interpreted
// as a ratio of the number of active sync users.
enum {
  SYNC_SIGN_IN_NEEDS_UPDATE = 1,
  SYNC_SERVICE_UNAVAILABLE,
  SYNC_NEEDS_PASSPHRASE,
  SYNC_UNRECOVERABLE_ERROR,
  SYNC_ERROR_COUNT
};

}  // namespace

namespace ios_internal {
namespace sync {

NSString* GetSyncErrorDescriptionForBrowserState(
    ios::ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kNoSyncServiceError:
      return nil;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION);
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_STATUS_UNRECOVERABLE_ERROR);
  }
}

NSString* GetSyncErrorMessageForBrowserState(
    ios::ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kNoSyncServiceError:
      return nil;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_INFO_OUT_OF_DATE);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_CONFIGURE_ENCRYPTION);
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return l10n_util::GetNSString(IDS_SYNC_SERVICE_UNAVAILABLE);
    case SyncSetupService::kSyncServiceCouldNotConnect:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_COULD_NOT_CONNECT);
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_UNRECOVERABLE);
  }
}

NSString* GetSyncErrorButtonTitleForBrowserState(
    ios::ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_UPDATE_CREDENTIALS);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE);
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_SIGN_IN_AGAIN);
    default:
      return nil;
  }
}

GenericChromeCommand* GetSyncCommandForBrowserState(
    ios::ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return [[[ShowSigninCommand alloc]
          initWithOperation:AUTHENTICATION_OPERATION_REAUTHENTICATE
          signInAccessPoint:signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN]
          autorelease];
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return [[[GenericChromeCommand alloc]
          initWithTag:IDC_SHOW_SYNC_PASSPHRASE_SETTINGS] autorelease];
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceUnrecoverableError:
    case SyncSetupService::kNoSyncServiceError:
      return [[[GenericChromeCommand alloc] initWithTag:IDC_SHOW_SYNC_SETTINGS]
          autorelease];
  }
}

bool displaySyncErrors(ios::ChromeBrowserState* browser_state, Tab* tab) {
  // Avoid displaying sync errors on incognito tabs.
  if (browser_state->IsOffTheRecord())
    return false;

  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  if (!syncSetupService)
    return false;
  SyncSetupService::SyncServiceState errorState =
      syncSetupService->GetSyncServiceState();
  if (IsTransientSyncError(errorState))
    return false;

  // Logs when an infobar is shown to user. See crbug/265352.
  int loggedErrorState = 0;
  switch (errorState) {
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      NOTREACHED();
      break;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      loggedErrorState = SYNC_SIGN_IN_NEEDS_UPDATE;
      break;
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      loggedErrorState = SYNC_NEEDS_PASSPHRASE;
      break;
    case SyncSetupService::kSyncServiceUnrecoverableError:
      loggedErrorState = SYNC_UNRECOVERABLE_ERROR;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.SyncErrorInfobarDisplayed", loggedErrorState,
                            SYNC_ERROR_COUNT);

  DCHECK(tab);
  DCHECK([tab infoBarManager]);
  return SyncErrorInfoBarDelegate::Create([tab infoBarManager], browser_state);
}

bool IsTransientSyncError(SyncSetupService::SyncServiceState errorState) {
  switch (errorState) {
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return true;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return false;
  }
}

}  // namespace sync
}  // namespace ios_internal
