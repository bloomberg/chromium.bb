// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller_private.h"

#import <StoreKit/StoreKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#import "ios/chrome/browser/installation_notifier.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/cells/native_app_item.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_whitelist_manager.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ios/web/public/web_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

const NSInteger kTagShift = 1000;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLearnMore = kSectionIdentifierEnumZero,
  SectionIdentifierApps,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeApp = kItemTypeEnumZero,
  ItemTypeLearnMore,
};

}  // namespace

@interface NativeAppsCollectionViewController ()<
    SKStoreProductViewControllerDelegate> {
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;
  base::scoped_nsobject<NSArray> _nativeAppsInSettings;
  BOOL _userDidSomething;
}

// List of the native apps visible in Settings.
@property(nonatomic, copy) NSArray* appsInSettings;

// Delegate for App-Store-related operations.
@property(nonatomic, assign) id<StoreKitLauncher> storeKitLauncher;

// Sets up the list of visible apps based on |nativeAppWhitelistManager|, which
// serves as datasource for this controller. Apps from
// |nativeAppWhitelistManager| are stored in |_nativeAppsInSettings| and
// |-reloadData| is sent to the receiver.
- (void)configureWithNativeAppWhiteListManager:
    (id<NativeAppWhitelistManager>)nativeAppWhitelistManager;

// Returns a new Native App collection view item for the metadata at |index| in
// |_nativeAppsInSettings|.
- (CollectionViewItem*)nativeAppItemAtIndex:(NSUInteger)index;

// Target method for the auto open in app switch.
// Called when an auto-open-in-app switch is toggled.
- (void)autoOpenInAppChanged:(UISwitch*)switchControl;

// Called when an Install button is being tapped.
- (void)installApp:(UIButton*)button;

// Called when an app with the registered scheme is opened.
- (void)appDidInstall:(NSNotification*)notification;

// Returns the app at |index| in the list of visible apps.
- (id<NativeAppMetadata>)nativeAppAtIndex:(NSUInteger)index;

// Records a user action in UMA under NativeAppLauncher.Settings.
// If this method is not called during the lifetime of the view,
// |settings::kNativeAppsActionDidNothing| is recorded in UMA.
- (void)recordUserAction:(settings::NativeAppsAction)action;

@end

@implementation NativeAppsCollectionViewController

@synthesize storeKitLauncher = _storeKitLauncher;

- (id)initWithURLRequestContextGetter:
    (net::URLRequestContextGetter*)requestContextGetter {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _imageFetcher = base::MakeUnique<image_fetcher::IOSImageDataFetcherWrapper>(
        requestContextGetter, web::WebThread::GetBlockingPool());
    base::RecordAction(base::UserMetricsAction("MobileGALOpenSettings"));
    _storeKitLauncher = self;

    [self loadModel];
  }
  return self;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  self.title = l10n_util::GetNSString(IDS_IOS_GOOGLE_APPS_SM_SETTINGS);
  [self configureWithNativeAppWhiteListManager:
            ios::GetChromeBrowserProvider()->GetNativeAppWhitelistManager()];

  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [[InstallationNotifier sharedInstance] checkNow];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(reloadData)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
  if ([self isMovingFromParentViewController]) {
    // The view controller is popped.
    [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
    if (!_userDidSomething)
      [self recordUserAction:settings::kNativeAppsActionDidNothing];
  }
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  NSUInteger appsCount = [_nativeAppsInSettings count];

  [model addSectionWithIdentifier:SectionIdentifierLearnMore];
  [model addItem:[self learnMoreItem]
      toSectionWithIdentifier:SectionIdentifierLearnMore];

  [model addSectionWithIdentifier:SectionIdentifierApps];

  for (NSUInteger i = 0; i < appsCount; i++) {
    [model addItem:[self nativeAppItemAtIndex:i]
        toSectionWithIdentifier:SectionIdentifierApps];
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  if ([self.collectionViewModel
          sectionIdentifierForSection:indexPath.section] ==
      SectionIdentifierApps) {
    NativeAppCell* appCell = base::mac::ObjCCastStrict<NativeAppCell>(cell);
    [self configureNativeAppCell:appCell atIndexPath:indexPath];
  }
  return cell;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(nonnull UICollectionView*)collectionView
    cellHeightAtIndexPath:(nonnull NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeLearnMore:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeApp:
      return MDCCellDefaultOneLineWithAvatarHeight;
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (void)configureNativeAppCell:(NativeAppCell*)appCell
                   atIndexPath:(NSIndexPath*)indexPath {
  appCell.switchControl.tag = [self tagForIndexPath:indexPath];
  [appCell.switchControl addTarget:self
                            action:@selector(autoOpenInAppChanged:)
                  forControlEvents:UIControlEventValueChanged];
  appCell.installButton.tag = [self tagForIndexPath:indexPath];
  [appCell.installButton addTarget:self
                            action:@selector(installApp:)
                  forControlEvents:UIControlEventTouchUpInside];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  NativeAppItem* appItem = base::mac::ObjCCastStrict<NativeAppItem>(item);
  if (!appItem.icon) {
    // Fetch the real icon.
    base::WeakNSObject<NativeAppsCollectionViewController> weakSelf(self);
    id<NativeAppMetadata> metadata = [self nativeAppAtIndex:indexPath.item];
    [metadata fetchSmallIconWithImageFetcher:_imageFetcher.get()
                             completionBlock:^(UIImage* image) {
                               base::scoped_nsobject<
                                   NativeAppsCollectionViewController>
                                   strongSelf([weakSelf retain]);
                               if (!image || !strongSelf)
                                 return;
                               appItem.icon = image;
                               [strongSelf.get().collectionView
                                   reloadItemsAtIndexPaths:@[ indexPath ]];
                             }];
  }
}

- (CollectionViewItem*)learnMoreItem {
  NSString* learnMoreText =
      l10n_util::GetNSString(IDS_IOS_GOOGLE_APPS_SM_SECTION_HEADER);
  CollectionViewFooterItem* learnMoreItem = [[[CollectionViewFooterItem alloc]
      initWithType:ItemTypeLearnMore] autorelease];
  learnMoreItem.text = learnMoreText;
  return learnMoreItem;
}

#pragma mark - SKStoreProductViewControllerDelegate methods

- (void)productViewControllerDidFinish:
    (SKStoreProductViewController*)viewController {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - StoreKitLauncher methods

- (void)openAppStore:(NSString*)appId {
  // Reported crashes show that -openAppStore: had been called with
  // a nil |appId|, but opening AppStore is meaningful only if the |appId| is
  // not nil, so be defensive and early return if |appId| is nil.
  if (![appId length])
    return;
  NSDictionary* product =
      @{SKStoreProductParameterITunesItemIdentifier : appId};
  base::scoped_nsobject<SKStoreProductViewController> storeViewController(
      [[SKStoreProductViewController alloc] init]);
  [storeViewController setDelegate:self];
  [storeViewController loadProductWithParameters:product completionBlock:nil];
  [self presentViewController:storeViewController animated:YES completion:nil];
}

#pragma mark - MDCCollectionViewStylingDelegate

// MDCCollectionViewStylingDelegate protocol is implemented so that cells don't
// display ink on touch.
- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(nonnull NSIndexPath*)indexPath {
  return YES;
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierLearnMore:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierLearnMore:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}
#pragma mark - Private methods

- (void)autoOpenInAppChanged:(UISwitch*)switchControl {
  NSInteger index = [self indexPathForTag:switchControl.tag].item;
  id<NativeAppMetadata> metadata = [self nativeAppAtIndex:index];
  DCHECK([metadata isInstalled]);
  BOOL autoOpenOn = switchControl.on;
  metadata.shouldAutoOpenLinks = autoOpenOn;
  [self recordUserAction:(autoOpenOn
                              ? settings::kNativeAppsActionTurnedAutoOpenOn
                              : settings::kNativeAppsActionTurnedAutoOpenOff)];
}

- (void)installApp:(UIButton*)button {
  [self recordUserAction:settings::kNativeAppsActionClickedInstall];
  NSInteger index = [self indexPathForTag:button.tag].item;
  id<NativeAppMetadata> metadata = [self nativeAppAtIndex:index];
  DCHECK(![metadata isInstalled]);
  [metadata updateCounterWithAppInstallation];

  // Register to get a notification when the app is installed.
  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(appDidInstall:)
                                 forScheme:[metadata anyScheme]];
  [self.storeKitLauncher openAppStore:[metadata appId]];
}

- (void)appDidInstall:(NSNotification*)notification {
  // The name of the notification is the scheme of the new app installed.
  GURL url(base::SysNSStringToUTF8([notification name]) + ":");
  DCHECK(url.is_valid());
  NSUInteger matchingAppIndex = [_nativeAppsInSettings
      indexOfObjectPassingTest:^(id obj, NSUInteger idx, BOOL* stop) {
        id<NativeAppMetadata> metadata =
            static_cast<id<NativeAppMetadata>>(obj);
        return [metadata canOpenURL:url];
      }];
  [[self nativeAppAtIndex:matchingAppIndex] setShouldAutoOpenLinks:YES];
  [self reloadData];
}

- (void)configureWithNativeAppWhiteListManager:
    (id<NativeAppWhitelistManager>)nativeAppWhitelistManager {
  NSArray* allApps = [nativeAppWhitelistManager
      filteredAppsUsingBlock:^(const id<NativeAppMetadata> app, BOOL* stop) {
        return [app isGoogleOwnedApp];
      }];
  [self setAppsInSettings:allApps];
  [self reloadData];
}

- (id<NativeAppMetadata>)nativeAppAtIndex:(NSUInteger)index {
  id<NativeAppMetadata> metadata = [_nativeAppsInSettings objectAtIndex:index];
  DCHECK([metadata conformsToProtocol:@protocol(NativeAppMetadata)]);
  return metadata;
}

- (void)recordUserAction:(settings::NativeAppsAction)action {
  _userDidSomething = YES;
  UMA_HISTOGRAM_ENUMERATION("NativeAppLauncher.Settings", action,
                            settings::kNativeAppsActionCount);
}

- (CollectionViewItem*)nativeAppItemAtIndex:(NSUInteger)index {
  id<NativeAppMetadata> metadata = [self nativeAppAtIndex:index];
  // Determine the state of the cell.
  NativeAppItemState state;
  if ([metadata isInstalled]) {
    state = [metadata shouldAutoOpenLinks] ? NativeAppItemSwitchOn
                                           : NativeAppItemSwitchOff;
  } else {
    state = NativeAppItemInstall;
  }
  NativeAppItem* appItem =
      [[[NativeAppItem alloc] initWithType:ItemTypeApp] autorelease];
  appItem.name = [metadata appName];
  appItem.state = state;
  return appItem;
}

- (NSArray*)appsInSettings {
  return _nativeAppsInSettings.get();
}

- (void)setAppsInSettings:(NSArray*)apps {
  _nativeAppsInSettings.reset([apps copy]);
}

- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath.section ==
         [self.collectionViewModel
             sectionForSectionIdentifier:SectionIdentifierApps]);
  return indexPath.item + kTagShift;
}

- (NSIndexPath*)indexPathForTag:(NSInteger)shiftedTag {
  NSInteger unshiftedTag = shiftedTag - kTagShift;
  return [NSIndexPath
      indexPathForItem:unshiftedTag
             inSection:[self.collectionViewModel
                           sectionForSectionIdentifier:SectionIdentifierApps]];
}

@end
