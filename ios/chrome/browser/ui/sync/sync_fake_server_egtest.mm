// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;

// Returns a fake identity.
ChromeIdentity* GetFakeIdentity1() {
  return [FakeChromeIdentity identityWithEmail:@"foo@gmail.com"
                                        gaiaID:@"fooID"
                                          name:@"Fake Foo"];
}

// Opens the signin screen from the settings page. Must be called from the NTP.
// User must not be signed in.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
void OpenSignInFromSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kSettingsSignInCellId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Signs in the identity for the specific |userEmail|. This is performed via the
// UI and must be called from the NTP.
void SignInIdentity(NSString* userEmail) {
  OpenSignInFromSettings();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   userEmail)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_DONE)] performAction:grey_tap()];
}

// Waits for sync to be initialized or not, based on |isSyncInitialized| and
// fails with a GREYAssert if that condition is never met.
void AssertSyncInitialized(bool is_initialized) {
  ConditionBlock condition = ^{
    return chrome_test_util::IsSyncInitialized() == is_initialized;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kSyncOperationTimeout, condition),
      @"Sync was not initialized");
}

// Waits for |entity_count| entities of type |entity_type|, and fails with
// a GREYAssert if the condition is not met, within a short period of time.
void AssertNumberOfEntities(int entity_count, syncer::ModelType entity_type) {
  ConditionBlock condition = ^{
    return chrome_test_util::GetNumberOfSyncEntities(entity_type) ==
           entity_count;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kSyncOperationTimeout, condition),
      @"Expected %d entities of the specified type", entity_count);
}

// Waits for |entity_count| entities of type |entity_type| with name |name|, and
// fails with a GREYAssert if the condition is not met, within a short period of
// time.
void AssertNumberOfEntitiesWithName(int entity_count,
                                    syncer::ModelType entity_type,
                                    std::string name) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    BOOL success = chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
        entity_type, name, entity_count, &error);
    DCHECK(success || error);
    return !!success;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kSyncOperationTimeout, condition),
      @"Expected %d entities of the specified type", entity_count);
}
}  // namespace

// Hermetic sync tests, which use the fake sync server.
@interface SyncFakeServerTestCase : ChromeTestCase
@end

@implementation SyncFakeServerTestCase

- (void)tearDown {
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout,
                 ^{
                   return chrome_test_util::BookmarksLoaded();
                 }),
             @"Bookmark model did not load");
  chrome_test_util::ClearBookmarks();
  AssertNumberOfEntities(0, syncer::BOOKMARKS);

  chrome_test_util::ClearSyncServerData();
  AssertNumberOfEntities(0, syncer::AUTOFILL_PROFILE);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  GREYAssertEqual(chrome_test_util::GetNumberOfSyncEntities(syncer::BOOKMARKS),
                  0, @"No bookmarks should exist before sync tests start.");
  GREYAssertEqual(chrome_test_util::GetNumberOfSyncEntities(syncer::TYPED_URLS),
                  0, @"No bookmarks should exist before sync tests start.");
}

// Tests that a bookmark added on the client (before Sync is enabled) is
// uploaded to the Sync server once Sync is turned on.
- (void)testSyncUploadBookmarkOnFirstSync {
  [self addBookmark:GURL("https://www.foo.com") withTitle:@"foo"];

  // Sign in to sync, after a bookmark has been added.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Assert that the correct number of bookmarks have been synced.
  AssertSyncInitialized(true);
  AssertNumberOfEntities(1, syncer::BOOKMARKS);
}

// Tests that a bookmark added on the client is uploaded to the Sync server.
- (void)testSyncUploadBookmark {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Add a bookmark after sync is initialized.
  AssertSyncInitialized(true);
  [self addBookmark:GURL("https://www.goo.com") withTitle:@"goo"];
  AssertNumberOfEntities(1, syncer::BOOKMARKS);
}

// Tests that a bookmark injected in the FakeServer is synced down to the
// client.
- (void)testSyncDownloadBookmark {
  [[self class] assertBookmarksWithTitle:@"hoo" expectedCount:0];
  chrome_test_util::InjectBookmarkOnFakeSyncServer("http://www.hoo.com", "hoo");

  // Sign in to sync, after a bookmark has been injected in the sync server.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);

  [[self class] assertBookmarksWithTitle:@"hoo" expectedCount:1];
}

// Tests that the local cache guid does not change when sync is restarted.
- (void)testSyncCheckSameCacheGuid_SyncRestarted {
  // Sign in the fake identity.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);

  // Store the original guid, then restart sync.
  std::string original_guid = chrome_test_util::GetSyncCacheGuid();
  chrome_test_util::StopSync();
  AssertSyncInitialized(false);
  chrome_test_util::StartSync();

  // Verify the guid did not change.
  AssertSyncInitialized(true);
  GREYAssertEqual(chrome_test_util::GetSyncCacheGuid(), original_guid,
                  @"Stored guid doesn't match current value");
}

// Tests that the local cache guid changes when the user signs out and then
// signs back in with the same account.
- (void)testSyncCheckDifferentCacheGuid_SignOutAndSignIn {
  // Sign in a fake identity, and store the initial sync guid.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);
  std::string original_guid = chrome_test_util::GetSyncCacheGuid();

  // Sign out the current user.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  GREYAssert(authentication_service->IsAuthenticated(),
             @"User is not signed in.");
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST, nil);
  AssertSyncInitialized(false);

  // Sign the user back in, and verify the guid has changed.
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);
  GREYAssertTrue(
      chrome_test_util::GetSyncCacheGuid() != original_guid,
      @"guid didn't change after user signed out and signed back in");
}

// Tests that the local cache guid does not change when sync is restarted, if
// a user previously signed out and back in.
// Test for http://crbug.com/413611 .
- (void)testSyncCheckSameCacheGuid_SyncRestartedAfterSignOutAndSignIn {
  // Sign in a fake idenitty.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);

  // Sign out the current user.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  GREYAssert(authentication_service->IsAuthenticated(),
             @"User is not signed in.");
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST, nil);
  AssertSyncInitialized(false);

  // Sign the user back in.
  SignInIdentity(identity.userEmail);
  AssertSyncInitialized(true);

  // Record the initial guid, before restarting sync.
  std::string original_guid = chrome_test_util::GetSyncCacheGuid();
  chrome_test_util::StopSync();
  AssertSyncInitialized(false);
  chrome_test_util::StartSync();

  // Verify the guid did not change after restarting sync.
  AssertSyncInitialized(true);
  GREYAssertEqual(chrome_test_util::GetSyncCacheGuid(), original_guid,
                  @"Stored guid doesn't match current value");
}

// Tests that autofill profile injected in FakeServer gets synced to client.
- (void)testSyncDownloadAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  GREYAssertFalse(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                  @"autofill profile should not exist");

  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(kGuid, kFullName);
  [self setTearDownHandler:^{
    chrome_test_util::ClearAutofillProfile(kGuid);
  }];

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Verify that the autofill profile has been downloaded.
  AssertSyncInitialized(YES);
  GREYAssertTrue(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                 @"autofill profile should exist");
}

// Test that update to autofill profile injected in FakeServer gets synced to
// client.
- (void)testSyncUpdateAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  const std::string kUpdatedFullName = "Roger Rabbit";
  GREYAssertFalse(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                  @"autofill profile should not exist");

  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(kGuid, kFullName);
  [self setTearDownHandler:^{
    chrome_test_util::ClearAutofillProfile(kGuid);
  }];

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Verify that the autofill profile has been downloaded.
  AssertSyncInitialized(YES);
  GREYAssertTrue(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                 @"autofill profile should exist");

  // Update autofill profile.
  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(kGuid,
                                                          kUpdatedFullName);

  // Trigger sync cycle and wait for update.
  chrome_test_util::TriggerSyncCycle(syncer::AUTOFILL_PROFILE);
  NSString* errorMessage =
      [NSString stringWithFormat:
                    @"Did not find autofill profile for guid: %@, and name: %@",
                    base::SysUTF8ToNSString(kGuid),
                    base::SysUTF8ToNSString(kUpdatedFullName)];
  ConditionBlock condition = ^{
    return chrome_test_util::IsAutofillProfilePresent(kGuid, kUpdatedFullName);
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kSyncOperationTimeout, condition),
      errorMessage);
}

// Test that autofill profile deleted from FakeServer gets deleted from client
// as well.
- (void)testSyncDeleteAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  GREYAssertFalse(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                  @"autofill profile should not exist");
  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(kGuid, kFullName);
  [self setTearDownHandler:^{
    chrome_test_util::ClearAutofillProfile(kGuid);
  }];

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Verify that the autofill profile has been downloaded
  AssertSyncInitialized(YES);
  GREYAssertTrue(chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName),
                 @"autofill profile should exist");

  // Delete autofill profile from server, and verify it is removed.
  chrome_test_util::DeleteAutofillProfileOnFakeSyncServer(kGuid);
  chrome_test_util::TriggerSyncCycle(syncer::AUTOFILL_PROFILE);
  ConditionBlock condition = ^{
    return !chrome_test_util::IsAutofillProfilePresent(kGuid, kFullName);
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kSyncOperationTimeout, condition),
      @"Autofill profile was not deleted.");
}

// Tests that tabs opened on this client are committed to the Sync server and
// that the created sessions entities are correct.
- (void)testSyncUploadOpenTabs {
  // Create map of canned responses and set up the test HTML server.
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://page1");
  const GURL URL2 = web::test::HttpServer::MakeUrl("http://page2");
  std::map<GURL, std::string> responses = {
      {URL1, std::string("page 1")}, {URL2, std::string("page 2")},
  };
  web::test::SetUpSimpleHttpServer(responses);

  // Load both URLs in separate tabs.
  [ChromeEarlGrey loadURL:URL1];
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:URL2];

  // Sign in to sync, after opening two tabs.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  // Verify the sessions on the sync server.
  AssertSyncInitialized(true);
  AssertNumberOfEntities(3, syncer::SESSIONS);

  NSError* error = nil;
  BOOL success = chrome_test_util::VerifySessionsOnSyncServer(
      std::multiset<std::string>{URL1.spec(), URL2.spec()}, &error);

  DCHECK(success || error);
  GREYAssertTrue(success, [error localizedDescription]);
}

// Tests that a typed URL (after Sync is enabled) is uploaded to the Sync
// server.
- (void)testSyncTypedURLUpload {
  const GURL mockURL("http://not-a-real-site/");

  chrome_test_util::ClearBrowsingHistory();

  [self setTearDownHandler:^{
    chrome_test_util::ClearBrowsingHistory();
  }];
  chrome_test_util::AddTypedURLOnClient(mockURL);

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  AssertSyncInitialized(YES);

  // Trigger sync and verify the typed URL is on the fake sync server.
  chrome_test_util::TriggerSyncCycle(syncer::TYPED_URLS);
  __block NSError* blockSafeError = nil;
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for typed URL to be uploaded."
                  block:^BOOL {
                    NSError* error = nil;
                    BOOL result =
                        chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
                            syncer::TYPED_URLS, mockURL.spec(), 1, &error);
                    blockSafeError = [error copy];
                    return result;
                  }];
  BOOL success = [condition waitWithTimeout:kSyncOperationTimeout];
  DCHECK(success || blockSafeError);
  GREYAssertTrue(success, [blockSafeError localizedDescription]);
}

// Tests that typed url is downloaded from sync server.
- (void)testSyncTypedUrlDownload {
  const GURL mockURL("http://not-a-real-site/");

  chrome_test_util::ClearBrowsingHistory();
  [self setTearDownHandler:^{
    chrome_test_util::ClearBrowsingHistory();
  }];

  // Inject typed url on server.
  chrome_test_util::InjectTypedURLOnFakeSyncServer(mockURL.spec());

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  AssertSyncInitialized(YES);

  // Wait for typed url to appear on client.
  __block NSError* blockSafeError = nil;

  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for typed URL to be downloaded."
                  block:^BOOL {
                    return chrome_test_util::IsTypedUrlPresentOnClient(
                        mockURL, YES, &blockSafeError);
                  }];
  BOOL success = [condition waitWithTimeout:kSyncOperationTimeout];
  DCHECK(success || blockSafeError);
  GREYAssert(success, [blockSafeError localizedDescription]);
}

// Tests that when typed url is deleted on the client, sync the change gets
// propagated to server.
- (void)testSyncTypedURLDeleteFromClient {
  const GURL mockURL("http://not-a-real-site/");

  chrome_test_util::ClearBrowsingHistory();

  [self setTearDownHandler:^{
    chrome_test_util::ClearBrowsingHistory();
  }];

  // Inject typed url on server.
  chrome_test_util::InjectTypedURLOnFakeSyncServer(mockURL.spec());

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  AssertSyncInitialized(YES);

  // Wait for typed url to appear on client.
  __block NSError* blockSafeError = nil;

  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for typed URL to be downloaded."
                  block:^BOOL {
                    return chrome_test_util::IsTypedUrlPresentOnClient(
                        mockURL, YES, &blockSafeError);
                  }];
  BOOL success = [condition waitWithTimeout:kSyncOperationTimeout];
  DCHECK(success || blockSafeError);
  GREYAssert(success, [blockSafeError localizedDescription]);

  GREYAssert(chrome_test_util::GetNumberOfSyncEntities(syncer::TYPED_URLS) == 1,
             @"There should be 1 typed URL entity");

  // Delete typed URL from client.
  chrome_test_util::DeleteTypedUrlFromClient(mockURL);

  // Trigger sync and wait for typed URL to be deleted.
  chrome_test_util::TriggerSyncCycle(syncer::TYPED_URLS);
  AssertNumberOfEntities(0, syncer::TYPED_URLS);
}

// Test that typed url is deleted from client after server sends tombstone for
// that typed url.
- (void)testSyncTypedURLDeleteFromServer {
  const GURL mockURL("http://not-a-real-site/");

  chrome_test_util::ClearBrowsingHistory();

  [self setTearDownHandler:^{
    chrome_test_util::ClearBrowsingHistory();
  }];
  chrome_test_util::AddTypedURLOnClient(mockURL);

  // Sign in to sync.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  SignInIdentity(identity.userEmail);

  AssertSyncInitialized(YES);
  chrome_test_util::TriggerSyncCycle(syncer::TYPED_URLS);

  AssertNumberOfEntitiesWithName(1, syncer::TYPED_URLS, mockURL.spec());

  chrome_test_util::DeleteTypedUrlFromClient(mockURL);

  // Trigger sync and wait for fake server to be updated.
  chrome_test_util::TriggerSyncCycle(syncer::TYPED_URLS);
  __block NSError* blockSafeError = nil;
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for typed URL to be downloaded."
                  block:^BOOL {
                    NSError* error = nil;
                    BOOL result = chrome_test_util::IsTypedUrlPresentOnClient(
                        mockURL, NO, &error);
                    blockSafeError = [error copy];
                    return result;
                  }];
  BOOL success = [condition waitWithTimeout:kSyncOperationTimeout];
  DCHECK(success || blockSafeError);
  GREYAssert(success, [blockSafeError localizedDescription]);
}

#pragma mark - Test Utilities

// Adds a bookmark with the given |url| and |title| into the Mobile Bookmarks
// folder.
// TODO(crbug.com/646164): This is copied from bookmarks_egtest.mm and should
// move to common location.
- (void)addBookmark:(const GURL)url withTitle:(NSString*)title {
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout,
                 ^{
                   return chrome_test_util::BookmarksLoaded();
                 }),
             @"Bookmark model did not load");
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(title), url);
}

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
// TODO(crbug.com/646164): This is copied from bookmarks_egtest.mm and should
// move to common location.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Verify the correct number of bookmarks exist.
  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  GREYAssertEqual(expectedCount, count, @"Unexpected number of bookmarks");
}

@end
