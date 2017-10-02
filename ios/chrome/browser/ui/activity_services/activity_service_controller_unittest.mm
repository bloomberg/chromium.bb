// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"
#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_password.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakePasswordFormFiller : NSObject<PasswordFormFiller>

// Stores the latest value passed to the invocation of the method
// -findAndFillPasswordForms:password:completionHandler:.
@property(nonatomic, readonly, copy) NSString* username;
@property(nonatomic, readonly, copy) NSString* password;

// YES if the method -findAndFillPasswordForms:password:completionHandler:
// was called on this object, NO otherwise.
@property(nonatomic, readonly, assign) BOOL methodCalled;

@end

@implementation FakePasswordFormFiller

@synthesize username = _username;
@synthesize password = _password;
@synthesize methodCalled = _methodCalled;

- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler {
  _methodCalled = YES;
  _username = [username copy];
  _password = [password copy];
  if (completionHandler)
    completionHandler(YES);
}

@end

@interface ActivityServiceController (CrVisibleForTesting)
- (NSArray*)activityItemsForData:(ShareToData*)data;
- (NSArray*)applicationActivitiesForData:(ShareToData*)data
                              dispatcher:(id<BrowserCommands>)dispatcher;

- (BOOL)processItemsReturnedFromActivity:(NSString*)activityType
                                  status:(ShareTo::ShareResult)result
                                   items:(NSArray*)extensionItems;
- (void)shareDidComplete:(ShareTo::ShareResult)shareStatus
       completionMessage:(NSString*)message;

// Setter function for mocking during testing
- (void)setProvidersForTesting:
            (id<ActivityServicePassword, ActivityServicePresentation>)provider
                    dispatcher:(id<SnackbarCommands>)dispatcher;
@end

@interface FakeActivityServiceControllerTestProvider
    : NSObject<ActivityServicePassword,
               ActivityServicePositioner,
               ActivityServicePresentation,
               SnackbarCommands>

@property(nonatomic, readonly, strong) UIViewController* parentViewController;
@property(nonatomic, readonly, strong)
    FakePasswordFormFiller* fakePasswordFormFiller;

// Tracks whether or not the associated provider methods were called.
@property(nonatomic, readonly, assign)
    BOOL presentActivityServiceViewControllerWasCalled;
@property(nonatomic, readonly, assign)
    BOOL activityServiceDidEndPresentingWasCalled;

// Stores the latest values that were passed to the associated provider methods.
@property(nonatomic, readonly, copy) NSString* latestErrorAlertTitle;
@property(nonatomic, readonly, copy) NSString* latestErrorAlertMessage;
@property(nonatomic, readonly, copy) NSString* latestSnackbarMessage;

// Resets the values of the properties above.
- (void)resetState;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation FakeActivityServiceControllerTestProvider

@synthesize presentActivityServiceViewControllerWasCalled =
    _presentActivityServiceViewControllerWasCalled;
@synthesize activityServiceDidEndPresentingWasCalled =
    _activityServiceDidEndPresentingWasCalled;
@synthesize latestErrorAlertTitle = _latestErrorAlertTitle;
@synthesize latestErrorAlertMessage = _latestErrorAlertMessage;
@synthesize latestSnackbarMessage = _latestSnackbarMessage;
@synthesize parentViewController = _parentViewController;
@synthesize fakePasswordFormFiller = _fakePasswordFormFiller;

- (instancetype)initWithParentViewController:(UIViewController*)controller {
  if ((self = [super init])) {
    _parentViewController = controller;
    _fakePasswordFormFiller = [[FakePasswordFormFiller alloc] init];
  }
  return self;
}

- (id<PasswordFormFiller>)currentPasswordFormFiller {
  return _fakePasswordFormFiller;
}

- (void)presentActivityServiceViewController:(UIViewController*)controller {
  _presentActivityServiceViewControllerWasCalled = YES;
  if (self.parentViewController) {
    [self.parentViewController presentViewController:controller
                                            animated:NO
                                          completion:nil];
  }
}

- (void)activityServiceDidEndPresenting {
  _activityServiceDidEndPresentingWasCalled = YES;
}

- (void)showErrorAlertWithStringTitle:(NSString*)title
                              message:(NSString*)message {
  _latestErrorAlertTitle = [title copy];
  _latestErrorAlertMessage = [message copy];
}

- (CGRect)shareButtonAnchorRect {
  // On iPad, UIPopovers must be anchored to rectangles that have a non zero
  // size.
  return CGRectMake(0, 0, 1, 1);
}

- (UIView*)shareButtonView {
  return self.parentViewController.view;
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
  _latestSnackbarMessage = [message.text copy];
}

- (void)resetState {
  _presentActivityServiceViewControllerWasCalled = NO;
  _activityServiceDidEndPresentingWasCalled = NO;
  _latestErrorAlertTitle = nil;
  _latestErrorAlertMessage = nil;
  _latestSnackbarMessage = nil;
}

@end

namespace {

class ActivityServiceControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    parentController_ =
        [[UIViewController alloc] initWithNibName:nil bundle:nil];
    [[UIApplication sharedApplication] keyWindow].rootViewController =
        parentController_;
    shareData_ =
        [[ShareToData alloc] initWithURL:GURL("https://chromium.org")
                                   title:@""
                         isOriginalTitle:YES
                         isPagePrintable:YES
                      thumbnailGenerator:DummyThumbnailGeneratorBlock()];
  }

  void TearDown() override {
    [[UIApplication sharedApplication] keyWindow].rootViewController = nil;
    PlatformTest::TearDown();
  }

  ThumbnailGeneratorBlock DummyThumbnailGeneratorBlock() {
    return ^UIImage*(CGSize const& size) { return nil; };
  }

  BOOL ArrayContainsImageSource(NSArray* array) {
    for (NSObject* item in array) {
      if ([item class] == [UIActivityImageSource class]) {
        return YES;
      }
    }
    return NO;
  }

  // Search |array| for id<UIActivityItemSource> objects. Returns an array of
  // matching NSExtensionItem objects returned by calling them.
  NSArray* FindItemsForActivityType(NSArray* array, NSString* activityType) {
    id mockActivityViewController =
        [OCMockObject niceMockForClass:[UIActivityViewController class]];
    NSMutableArray* result = [NSMutableArray array];
    for (id item in array) {
      if ([item respondsToSelector:@selector(activityViewController:
                                                itemForActivityType:)]) {
        id resultItem = [item activityViewController:mockActivityViewController
                                 itemForActivityType:activityType];
        if ([resultItem isKindOfClass:[NSExtensionItem class]])
          [result addObject:resultItem];
      }
    }
    return result;
  }

  // Searches |array| for objects of class |klass| and returns them in an
  // autoreleased array.
  NSArray* FindItemsOfClass(NSArray* array, Class klass) {
    NSMutableArray* result = [NSMutableArray array];
    for (id item in array) {
      if ([item isKindOfClass:klass])
        [result addObject:item];
    }
    return result;
  }

  // Searches |array| for objects returning |inUTType| conforming objects.
  // Returns an autoreleased array of conforming objects.
  NSArray* FindItemsEqualsToUTType(NSArray* array,
                                   NSString* activityType,
                                   NSString* inUTType) {
    id mockActivityViewController =
        [OCMockObject niceMockForClass:[UIActivityViewController class]];
    NSMutableArray* result = [NSMutableArray array];
    for (id item in array) {
      if (![item conformsToProtocol:@protocol(UIActivityItemSource)])
        continue;
      SEL dataTypeSelector =
          @selector(activityViewController:dataTypeIdentifierForActivityType:);
      if (![item respondsToSelector:dataTypeSelector])
        continue;
      NSString* itemDataType =
          [item activityViewController:mockActivityViewController
              dataTypeIdentifierForActivityType:activityType];
      if ([itemDataType isEqualToString:inUTType]) {
        [result addObject:item];
      }
    }
    return result;
  }

  // Calls -processItemsReturnedFromActivity:status:items: with the provided
  // |extensionItem| and expects failure.
  void ProcessItemsReturnedFromActivityFailure(NSArray* extensionItems,
                                               BOOL expectedResetUI) {
    ActivityServiceController* activityController =
        [[ActivityServiceController alloc] init];
    FakeActivityServiceControllerTestProvider* provider =
        [[FakeActivityServiceControllerTestProvider alloc]
            initWithParentViewController:nil];
    [activityController setProvidersForTesting:provider dispatcher:provider];

    // The following call to |processItemsReturnedFromActivity| should not
    // trigger any calls to the PasswordFormFiller.
    EXPECT_TRUE(provider.fakePasswordFormFiller);
    EXPECT_FALSE(provider.fakePasswordFormFiller.methodCalled);

    // Sets up the returned item from a Password Management App Extension.
    NSString* activityType = @"com.lastpass.ilastpass.LastPassExt";
    ShareTo::ShareResult result = ShareTo::ShareResult::SHARE_SUCCESS;
    BOOL resetUI =
        [activityController processItemsReturnedFromActivity:activityType
                                                      status:result
                                                       items:extensionItems];
    ASSERT_EQ(expectedResetUI, resetUI);

    EXPECT_TRUE(provider.fakePasswordFormFiller);
    EXPECT_FALSE(provider.fakePasswordFormFiller.methodCalled);
  }

  web::TestWebThreadBundle thread_bundle_;
  UIViewController* parentController_;
  ShareToData* shareData_;
};

TEST_F(ActivityServiceControllerTest, PresentAndDismissController) {
  UIViewController* parentController =
      static_cast<UIViewController*>(parentController_);

  FakeActivityServiceControllerTestProvider* provider =
      [[FakeActivityServiceControllerTestProvider alloc]
          initWithParentViewController:parentController];
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];
  EXPECT_FALSE([activityController isActive]);

  // Test sharing.
  [activityController shareWithData:shareData_
                       browserState:nullptr
                         dispatcher:nil
                   passwordProvider:provider
                   positionProvider:provider
               presentationProvider:provider];
  EXPECT_TRUE(provider.presentActivityServiceViewControllerWasCalled);
  EXPECT_FALSE(provider.activityServiceDidEndPresentingWasCalled);
  EXPECT_TRUE([activityController isActive]);

  // Cancels sharing and isActive flag should be turned off.
  [activityController cancelShareAnimated:NO];
  base::test::ios::WaitUntilCondition(^bool() {
    return ![activityController isActive];
  });
  EXPECT_TRUE(provider.activityServiceDidEndPresentingWasCalled);
}

// Verifies that an UIActivityImageSource is sent to the
// UIActivityViewController if and only if the ShareToData contains an image.
TEST_F(ActivityServiceControllerTest, ActivityItemsForData) {
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];

  // ShareToData does not contain an image, so the result items array will not
  // contain an image source.
  ShareToData* data =
      [[ShareToData alloc] initWithURL:GURL("https://chromium.org")
                                 title:@"foo"
                       isOriginalTitle:YES
                       isPagePrintable:YES
                    thumbnailGenerator:DummyThumbnailGeneratorBlock()];
  NSArray* items = [activityController activityItemsForData:data];
  EXPECT_FALSE(ArrayContainsImageSource(items));

  // Adds an image to the ShareToData object and call -activityItemsForData:
  // again. Verifies that the result items array contains an image source.
  [data setImage:[UIImage imageNamed:@"activity_services_print"]];
  items = [activityController activityItemsForData:data];
  EXPECT_TRUE(ArrayContainsImageSource(items));
}

// Verifies that when App Extension support is enabled, the URL string is
// passed in a dictionary as part of the Activity Items to the App Extension.
TEST_F(ActivityServiceControllerTest, ActivityItemsForDataWithPasswordAppEx) {
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];
  ShareToData* data =
      [[ShareToData alloc] initWithURL:GURL("https://chromium.org/login.html")
                                 title:@"kung fu fighting"
                       isOriginalTitle:YES
                       isPagePrintable:YES
                    thumbnailGenerator:DummyThumbnailGeneratorBlock()];
  NSArray* items = [activityController activityItemsForData:data];
  NSString* findLoginAction =
      (NSString*)activity_services::kUTTypeAppExtensionFindLoginAction;
  // Gets the list of NSExtensionItem objects returned by the array of
  // id<UIActivityItemSource> objects returned by -activityItemsForData:.
  NSArray* extensionItems = FindItemsForActivityType(
      items, @"com.agilebits.onepassword-ios.extension");
  ASSERT_EQ(1U, [extensionItems count]);
  NSExtensionItem* item = extensionItems[0];
  EXPECT_EQ(1U, item.attachments.count);
  NSItemProvider* itemProvider = item.attachments[0];
  // Extracts the dictionary back from the ItemProvider and then check that
  // it has the expected version and the page's URL.
  __block NSDictionary* result;
  [itemProvider
      loadItemForTypeIdentifier:findLoginAction
                        options:nil
              completionHandler:^(id item, NSError* error) {
                if (error || ![item isKindOfClass:[NSDictionary class]]) {
                  result = @{};
                } else {
                  result = item;
                }
              }];
  base::test::ios::WaitUntilCondition(^{
    return result != nil;
  });
  EXPECT_EQ(2U, [result count]);
  // Checks version.
  NSNumber* version =
      [result objectForKey:activity_services::kPasswordAppExVersionNumberKey];
  EXPECT_NSEQ(activity_services::kPasswordAppExVersionNumber, version);
  // Checks URL.
  NSString* appExUrlString =
      [result objectForKey:activity_services::kPasswordAppExURLStringKey];
  EXPECT_NSEQ(@"https://chromium.org/login.html", appExUrlString);

  // Checks that the list includes the page's title.
  NSArray* sources = FindItemsOfClass(items, [UIActivityURLSource class]);
  EXPECT_EQ(1U, [sources count]);
  UIActivityURLSource* actionSource = sources[0];
  id mockActivityViewController =
      [OCMockObject niceMockForClass:[UIActivityViewController class]];
  NSString* title = [actionSource
      activityViewController:mockActivityViewController
      subjectForActivityType:@"com.agilebits.onepassword-ios.extension"];
  EXPECT_NSEQ(@"kung fu fighting", title);
}

// Verifies that a Share extension can fetch a URL when Password App Extension
// is enabled.
TEST_F(ActivityServiceControllerTest,
       ActivityItemsForDataWithPasswordAppExReturnsURL) {
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];
  ShareToData* data =
      [[ShareToData alloc] initWithURL:GURL("https://chromium.org/login.html")
                                 title:@"kung fu fighting"
                       isOriginalTitle:YES
                       isPagePrintable:YES
                    thumbnailGenerator:DummyThumbnailGeneratorBlock()];
  NSArray* items = [activityController activityItemsForData:data];
  NSString* shareAction = @"com.apple.UIKit.activity.PostToFacebook";
  NSArray* urlItems =
      FindItemsEqualsToUTType(items, shareAction, @"public.url");
  ASSERT_EQ(1U, [urlItems count]);
  id<UIActivityItemSource> itemSource = urlItems[0];
  id mockActivityViewController =
      [OCMockObject niceMockForClass:[UIActivityViewController class]];
  id item = [itemSource activityViewController:mockActivityViewController
                           itemForActivityType:shareAction];
  ASSERT_TRUE([item isKindOfClass:[NSURL class]]);
  EXPECT_NSEQ(@"https://chromium.org/login.html", [item absoluteString]);
}

// Verifies that -processItemsReturnedFromActivity:status:item: contains
// the username and password.
TEST_F(ActivityServiceControllerTest, ProcessItemsReturnedSuccessfully) {
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];

  FakeActivityServiceControllerTestProvider* provider =
      [[FakeActivityServiceControllerTestProvider alloc]
          initWithParentViewController:nil];
  ASSERT_TRUE([provider currentPasswordFormFiller]);
  [activityController setProvidersForTesting:provider dispatcher:nil];

  EXPECT_TRUE(provider.fakePasswordFormFiller);
  EXPECT_FALSE(provider.fakePasswordFormFiller.methodCalled);

  // Sets up expectations on the mock PasswordController to check that the
  // callback function is called with the correct username and password.
  NSString* const kSecretUsername = @"john.doe";
  NSString* const kSecretPassword = @"super!secret";

  // Sets up the returned item from a Password Management App Extension.
  NSString* activityType = @"com.software.find-login-action.extension";
  ShareTo::ShareResult result = ShareTo::ShareResult::SHARE_SUCCESS;
  NSDictionary* dictionaryFromAppEx =
      @{ @"username" : kSecretUsername,
         @"password" : kSecretPassword };
  NSItemProvider* itemProvider =
      [[NSItemProvider alloc] initWithItem:dictionaryFromAppEx
                            typeIdentifier:(NSString*)kUTTypePropertyList];
  NSExtensionItem* extensionItem = [[NSExtensionItem alloc] init];
  [extensionItem setAttachments:@[ itemProvider ]];

  BOOL resetUI =
      [activityController processItemsReturnedFromActivity:activityType
                                                    status:result
                                                     items:@[ extensionItem ]];
  ASSERT_FALSE(resetUI);

  // Wait for the -findAndFillPasswordForms:password:completionHandler: method
  // to be called on the FakePasswordFormFiller.
  base::test::ios::WaitUntilCondition(^bool() {
    return provider.fakePasswordFormFiller.methodCalled;
  });

  EXPECT_NSEQ(kSecretUsername, provider.fakePasswordFormFiller.username);
  EXPECT_NSEQ(kSecretPassword, provider.fakePasswordFormFiller.password);
}

// Verifies that -processItemsReturnedFromActivity:status:item: fails when
// called with invalid NSExtensionItem.
TEST_F(ActivityServiceControllerTest, ProcessItemsReturnedFailures) {
  ProcessItemsReturnedFromActivityFailure(@[], YES);

  // Extension Item is empty.
  NSExtensionItem* extensionItem = [[NSExtensionItem alloc] init];
  [extensionItem setAttachments:@[]];
  ProcessItemsReturnedFromActivityFailure(@[ extensionItem ], YES);

  // Extension Item does not have a property list provider as the first
  // attachment.
  NSItemProvider* itemProvider =
      [[NSItemProvider alloc] initWithItem:@"some arbitrary garbage"
                            typeIdentifier:(NSString*)kUTTypeText];
  [extensionItem setAttachments:@[ itemProvider ]];
  ProcessItemsReturnedFromActivityFailure(@[ extensionItem ], YES);

  // Property list provider did not return a dictionary object.
  itemProvider =
      [[NSItemProvider alloc] initWithItem:@[ @"foo", @"bar" ]
                            typeIdentifier:(NSString*)kUTTypePropertyList];
  [extensionItem setAttachments:@[ itemProvider ]];
  ProcessItemsReturnedFromActivityFailure(@[ extensionItem ], NO);
}

// Verifies that the PrintActivity is sent to the UIActivityViewController if
// and only if the activity is "printable".
TEST_F(ActivityServiceControllerTest, ApplicationActivitiesForData) {
  ActivityServiceController* activityController =
      [[ActivityServiceController alloc] init];

  // Verify printable data.
  ShareToData* data =
      [[ShareToData alloc] initWithURL:GURL("https://chromium.org/printable")
                                 title:@"bar"
                       isOriginalTitle:YES
                       isPagePrintable:YES
                    thumbnailGenerator:DummyThumbnailGeneratorBlock()];

  NSArray* items = [activityController applicationActivitiesForData:data
                                                         dispatcher:nil];
  ASSERT_EQ(2U, [items count]);
  EXPECT_EQ([PrintActivity class], [[items objectAtIndex:0] class]);

  // Verify non-printable data.
  data =
      [[ShareToData alloc] initWithURL:GURL("https://chromium.org/unprintable")
                                 title:@"baz"
                       isOriginalTitle:YES
                       isPagePrintable:NO
                    thumbnailGenerator:DummyThumbnailGeneratorBlock()];
  items = [activityController applicationActivitiesForData:data
                                                dispatcher:nil];
  EXPECT_EQ(1U, [items count]);
}

TEST_F(ActivityServiceControllerTest, FindLoginActionTypeConformsToPublicURL) {
  // If this test fails, it is probably due to missing or incorrect
  // UTImportedTypeDeclarations in Info.plist. Note that there are
  // two Info.plist,
  // - ios/chrome/app/resources/Info.plist for Chrome app
  // - testing/gtest_ios/unittest-Info.plist for ios_chrome_unittests
  // Both of them must be changed.

  // 1Password defined the type @"org.appextension.find-login-action" so
  // any app can launch the 1Password app extension to fill in username and
  // password. This is being used by iOS native apps to launch 1Password app
  // extension and show *only* 1Password app extension as an option.
  // Therefore, this data type should *not* conform to public.url.
  // During the transition period, this test:
  // EXPECT_FALSE(UTTypeConformsTo(onePasswordFindLoginAction, kUTTypeURL));
  // is not possible due to backward compatibility configurations.
  CFStringRef onePasswordFindLoginAction =
      reinterpret_cast<CFStringRef>(@"org.appextension.find-login-action");

  // Chrome defines kUTTypeAppExtensionFindLoginAction which conforms to
  // public.url UTType in order to allow Share actions (e.g. Facebook, Twitter,
  // etc) to appear on UIActivityViewController opened by Chrome).
  CFStringRef chromeFindLoginAction = reinterpret_cast<CFStringRef>(
      activity_services::kUTTypeAppExtensionFindLoginAction);
  EXPECT_TRUE(UTTypeConformsTo(chromeFindLoginAction, kUTTypeURL));
  EXPECT_TRUE(
      UTTypeConformsTo(chromeFindLoginAction, onePasswordFindLoginAction));
}

// Verifies that the snackbar provider is invoked to show the given success
// message on receiving a -shareDidComplete callback for a successful share.
TEST_F(ActivityServiceControllerTest, TestShareDidCompleteWithSuccess) {
  ActivityServiceController* controller =
      [[ActivityServiceController alloc] init];
  FakeActivityServiceControllerTestProvider* provider =
      [[FakeActivityServiceControllerTestProvider alloc]
          initWithParentViewController:nil];
  [controller setProvidersForTesting:provider dispatcher:provider];

  NSString* completion_message = @"Completion!";
  [controller shareDidComplete:ShareTo::SHARE_SUCCESS
             completionMessage:completion_message];

  EXPECT_TRUE(provider.activityServiceDidEndPresentingWasCalled);
  EXPECT_NSEQ(completion_message, provider.latestSnackbarMessage);
}

// Verifies that the snackbar and error alert providers are not invoked for a
// cancelled share.
TEST_F(ActivityServiceControllerTest, TestShareDidCompleteWithCancellation) {
  ActivityServiceController* controller =
      [[ActivityServiceController alloc] init];
  FakeActivityServiceControllerTestProvider* provider =
      [[FakeActivityServiceControllerTestProvider alloc]
          initWithParentViewController:nil];
  [controller setProvidersForTesting:provider dispatcher:provider];

  [controller shareDidComplete:ShareTo::SHARE_CANCEL
             completionMessage:@"dummy"];
  EXPECT_TRUE(provider.activityServiceDidEndPresentingWasCalled);
  EXPECT_FALSE(provider.latestErrorAlertTitle);
  EXPECT_FALSE(provider.latestErrorAlertMessage);
  EXPECT_FALSE(provider.latestSnackbarMessage);
}

// Verifies that the error alert provider is invoked with the proper error
// message upon receiving a -shareDidComplete callback for a failed share.
TEST_F(ActivityServiceControllerTest, TestShareDidCompleteWithError) {
  ActivityServiceController* controller =
      [[ActivityServiceController alloc] init];
  FakeActivityServiceControllerTestProvider* provider =
      [[FakeActivityServiceControllerTestProvider alloc]
          initWithParentViewController:nil];
  [controller setProvidersForTesting:provider dispatcher:provider];

  [controller shareDidComplete:ShareTo::SHARE_ERROR completionMessage:@"dummy"];

  NSString* error_title =
      l10n_util::GetNSString(IDS_IOS_SHARE_TO_ERROR_ALERT_TITLE);
  NSString* error_message =
      l10n_util::GetNSString(IDS_IOS_SHARE_TO_ERROR_ALERT);
  EXPECT_NSEQ(error_title, provider.latestErrorAlertTitle);
  EXPECT_NSEQ(error_message, provider.latestErrorAlertMessage);
  EXPECT_FALSE(provider.latestSnackbarMessage);
}

}  // namespace
