// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_field_trial_version.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/search_widget_extension/copied_content_view.h"
#import "ios/chrome/search_widget_extension/search_widget_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Using GURL in the extension is not wanted as it includes ICU which makes the
// extension binary much larger; therefore, ios/chrome/common/x_callback_url.h
// cannot be used. This class makes a very basic use of x-callback-url, so no
// full implementation is required.
NSString* const kXCallbackURLHost = @"x-callback-url";
}  // namespace

@interface SearchWidgetViewController ()<SearchWidgetViewActionTarget>
@property(nonatomic, weak) SearchWidgetView* widgetView;
@property(nonatomic, strong, nullable) NSString* copiedText;
@property(nonatomic) CopiedContentType copiedContentType;
@property(nonatomic, strong)
    ClipboardRecentContentImplIOS* clipboardRecentContent;
@property(nonatomic, copy, nullable) NSDictionary* fieldTrialValues;
@property(nonatomic, readonly) BOOL copiedContentBehaviorEnabled;

// Updates the widget with latest data from the clipboard. Returns whether any
// visual updates occurred.
- (BOOL)updateWidget;
// Opens the main application with the given |command|.
- (void)openAppWithCommand:(NSString*)command;
// Opens the main application with the given |command| and |text|.
- (void)openAppWithCommand:(NSString*)command text:(NSString*)text;
// Returns the dictionary of commands to pass via user defaults to open the main
// application for a given |command| and optional |text|.
+ (NSDictionary*)dictForCommand:(NSString*)command text:(NSString*)text;
// Register a display of the widget in the app_group NSUserDefaults.
// Metrics on the widget usage will be sent (if enabled) on the next Chrome
// startup.
- (void)registerWidgetDisplay;
// Sets the copied content type. |copiedText| should be provided if the content
// type requires textual data, otherwise it should be nil. Also saves the data
// and returns YES if the screen needs updating and NO otherwise.
- (BOOL)setCopiedContentType:(CopiedContentType)type
                  copiedText:(NSString*)copiedText;

@end

@implementation SearchWidgetViewController

@synthesize widgetView = _widgetView;
@synthesize copiedText = _copiedText;
@synthesize copiedContentType = _copiedContentType;
@synthesize clipboardRecentContent = _clipboardRecentContent;

- (instancetype)init {
  self = [super init];
  if (self) {
    _clipboardRecentContent = [[ClipboardRecentContentImplIOS alloc]
           initWithMaxAge:1 * 60 * 60
        authorizedSchemes:[NSSet setWithObjects:@"http", @"https", nil]
             userDefaults:app_group::GetGroupUserDefaults()
                 delegate:nil];
    _copiedContentType = CopiedContentTypeNone;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  DCHECK(self.extensionContext);

  CGFloat height =
      [self.extensionContext
          widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact]
          .height;

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  SearchWidgetView* widgetView =
      [[SearchWidgetView alloc] initWithActionTarget:self compactHeight:height];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];
  [self updateWidget];

  self.extensionContext.widgetLargestAvailableDisplayMode =
      NCWidgetDisplayModeExpanded;

  self.widgetView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(self.view, self.widgetView);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self registerWidgetDisplay];
  [self updateWidget];

  // |widgetActiveDisplayMode| does not contain a valid value in viewDidLoad. By
  // the time viewWillAppear is called, it is correct, so set the mode here.
  BOOL initiallyCompact = [self.extensionContext widgetActiveDisplayMode] ==
                          NCWidgetDisplayModeCompact;
  [self.widgetView showMode:initiallyCompact];
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  completionHandler([self updateWidget] ? NCUpdateResultNewData
                                        : NCUpdateResultNoData);
}
- (BOOL)updateWidget {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* fieldTrialKey =
      base::SysUTF8ToNSString(app_group::kChromeExtensionFieldTrialPreference);
  self.fieldTrialValues = [sharedDefaults dictionaryForKey:fieldTrialKey];

  NSString* copiedText;
  CopiedContentType type = CopiedContentTypeNone;

  if (NSURL* url = [self.clipboardRecentContent recentURLFromClipboard]) {
    copiedText = url.absoluteString;
    type = CopiedContentTypeURL;
  } else if (NSString* text = [self getCopiedTextUsingFlag]) {
    copiedText = text;
    type = CopiedContentTypeString;
  }

  return [self setCopiedContentType:type copiedText:copiedText];
}

// Helper method to encapsulate both checking the flag and getting the copied
// text.
// TODO(crbug.com/932116): Can be removed when the flag is cleaned up.
- (NSString*)getCopiedTextUsingFlag {
  if (!self.copiedContentBehaviorEnabled) {
    return nil;
  }
  return [self.clipboardRecentContent recentTextFromClipboard];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  BOOL isCompact = [self.extensionContext widgetActiveDisplayMode] ==
                   NCWidgetDisplayModeCompact;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self.widgetView showMode:isCompact];
        [self.widgetView layoutIfNeeded];
      }
                      completion:nil];
}

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize
    API_AVAILABLE(ios(10.0)) {
  switch (activeDisplayMode) {
    case NCWidgetDisplayModeCompact:
      self.preferredContentSize = maxSize;
      break;
    case NCWidgetDisplayModeExpanded:
      self.preferredContentSize =
          CGSizeMake(maxSize.width, [self.widgetView widgetHeight]);
      break;
  }
}

#pragma mark - SearchWidgetViewActionTarget

- (void)openSearch:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupFocusOmniboxCommand)];
}

- (void)openIncognito:(id)sender {
  [self
      openAppWithCommand:base::SysUTF8ToNSString(
                             app_group::kChromeAppGroupIncognitoSearchCommand)];
}

- (void)openVoice:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupVoiceSearchCommand)];
}

- (void)openQRCode:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupQRScannerCommand)];
}

- (void)openCopiedContent:(id)sender {
  DCHECK(self.copiedText);
  NSString* command;
  switch (self.copiedContentType) {
    case CopiedContentTypeURL:
      command =
          base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand);
      break;
    case CopiedContentTypeString:
      command =
          base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand);
      break;
    case CopiedContentTypeNone:
      NOTREACHED();
      return;
  }
  [self openAppWithCommand:command text:self.copiedText];
}

#pragma mark - internal

- (void)openAppWithCommand:(NSString*)command {
  return [self openAppWithCommand:command text:nil];
}

- (void)registerWidgetDisplay {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger numberOfDisplay =
      [sharedDefaults integerForKey:app_group::kSearchExtensionDisplayCount];
  [sharedDefaults setInteger:numberOfDisplay + 1
                      forKey:app_group::kSearchExtensionDisplayCount];
}

- (void)openAppWithCommand:(NSString*)command text:(NSString*)text {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* defaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  [sharedDefaults setObject:[SearchWidgetViewController dictForCommand:command
                                                                  text:text]
                     forKey:defaultsKey];
  [sharedDefaults synchronize];

  NSString* scheme = base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return;

  NSURLComponents* urlComponents = [NSURLComponents new];
  urlComponents.scheme = scheme;
  urlComponents.host = kXCallbackURLHost;
  urlComponents.path = [NSString
      stringWithFormat:@"/%@", base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupXCallbackCommand)];

  NSURL* openURL = [urlComponents URL];
  [self.extensionContext openURL:openURL completionHandler:nil];
}

+ (NSDictionary*)dictForCommand:(NSString*)command text:(NSString*)text {
  NSString* timePrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* appPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);

  if (text) {
    NSString* textPrefKey = base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandTextPreference);
    return @{
      timePrefKey : [NSDate date],
      appPrefKey : app_group::kOpenCommandSourceSearchExtension,
      commandPrefKey : command,
      textPrefKey : text,
    };
  }
  return @{
    timePrefKey : [NSDate date],
    appPrefKey : app_group::kOpenCommandSourceSearchExtension,
    commandPrefKey : command,
  };
}

- (BOOL)setCopiedContentType:(CopiedContentType)type
                  copiedText:(NSString*)copiedText {
  if (self.copiedContentType == type &&
      [self.copiedText isEqualToString:copiedText]) {
    return NO;
  }
  self.copiedContentType = type;
  self.copiedText = copiedText;
  [self.widgetView setCopiedContentType:self.copiedContentType
                             copiedText:self.copiedText];
  return YES;
}

- (BOOL)copiedContentBehaviorEnabled {
  NSDictionary* storedData = self.fieldTrialValues[@"CopiedContentBehavior"];
  if (![kCopiedContentBehaviorVersion
          isEqualToNumber:storedData[kFieldTrialVersionKey]]) {
    return NO;
  }

  return [storedData[kFieldTrialValueKey] boolValue];
}

@end
