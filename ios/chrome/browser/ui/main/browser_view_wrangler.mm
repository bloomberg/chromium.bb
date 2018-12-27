// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/main/browser_coordinator.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Internal implementation of BrowserInterface -- for the most part a wrapper
// around BrowserCoordinator.
@interface WrangledBrowser : NSObject <BrowserInterface>

@property(nonatomic, weak, readonly) BrowserCoordinator* coordinator;

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator;

@end

@implementation WrangledBrowser

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator {
  if (self = [super init]) {
    _coordinator = coordinator;
  }
  return self;
}

- (UIViewController*)viewController {
  return self.coordinator.viewController;
}

- (BrowserViewController*)bvc {
  return self.coordinator.viewController;
}

- (TabModel*)tabModel {
  return self.coordinator.tabModel;
}

- (ios::ChromeBrowserState*)browserState {
  return self.coordinator.viewController.browserState;
}

- (BOOL)userInteractionEnabled {
  return self.coordinator.active;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  self.coordinator.active = userInteractionEnabled;
}

- (BOOL)incognito {
  return self.browserState->IsOffTheRecord();
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.coordinator clearPresentedStateWithCompletion:completion
                                       dismissOmnibox:dismissOmnibox];
}

@end

@interface BrowserViewWrangler ()<TabModelObserver> {
  ios::ChromeBrowserState* _browserState;
  __weak id<TabModelObserver> _tabModelObserver;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  __weak id<BrowserStateStorageSwitching> _storageSwitcher;
  BOOL _isShutdown;
}

@property(nonatomic, strong, readwrite) WrangledBrowser* mainInterface;
@property(nonatomic, strong, readwrite) WrangledBrowser* incognitoInterface;

// Backing objects.
@property(nonatomic) BrowserCoordinator* mainBrowserCoordinator;
@property(nonatomic) BrowserCoordinator* incognitoBrowserCoordinator;
@property(nonatomic) TabModel* mainTabModel;
@property(nonatomic) TabModel* otrTabModel;

// Responsible for maintaining all state related to sharing to other devices.
// Redeclared readwrite from the readonly declaration in the Testing interface.
@property(nonatomic, strong, readwrite)
    DeviceSharingManager* deviceSharingManager;

// Creates a new autoreleased tab model for |browserState|; if |empty| is NO,
// then any existing tabs that have been saved for |browserState| will be
// loaded; otherwise, the tab model will be created empty.
- (TabModel*)tabModelForBrowserState:(ios::ChromeBrowserState*)browserState
                               empty:(BOOL)empty;

// Creates a new off-the-record ("incognito") browser state for |_browserState|,
// then calls -tabModelForBrowserState:empty: and returns the (autoreleased)
// result.
- (TabModel*)buildOtrTabModel:(BOOL)empty;

// Creates the correct BrowserCoordinator for the corresponding browser state
// and tab model.
- (BrowserCoordinator*)coordinatorForBrowserState:
                           (ios::ChromeBrowserState*)browserState
                                         tabModel:(TabModel*)tabModel;
@end

@implementation BrowserViewWrangler

@synthesize currentInterface = _currentInterface;
@synthesize mainTabModel = _mainTabModel;
@synthesize otrTabModel = _otrTabModel;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                    tabModelObserver:(id<TabModelObserver>)tabModelObserver
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
                     storageSwitcher:
                         (id<BrowserStateStorageSwitching>)storageSwitcher {
  if ((self = [super init])) {
    _browserState = browserState;
    _tabModelObserver = tabModelObserver;
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _storageSwitcher = storageSwitcher;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

- (void)createMainTabModel {
  self.mainTabModel = [self tabModelForBrowserState:_browserState empty:NO];
  // Follow loaded URLs in the main tab model to send those in case of
  // crashes.
  breakpad::MonitorURLsForTabModel(_mainTabModel);
  ios::GetChromeBrowserProvider()->InitializeCastService(_mainTabModel);
}

#pragma mark - BrowserViewInformation property implementations

- (void)setCurrentInterface:(WrangledBrowser*)interface {
  DCHECK(interface);
  // |interface| must be one of the interfaces this class already owns.
  DCHECK(self.mainInterface == interface ||
         self.incognitoInterface == interface);
  if (self.currentInterface == interface) {
    return;
  }

  if (self.currentInterface) {
    // Tell the current BVC it moved to the background.
    [self.currentInterface.bvc setPrimary:NO];

    // Data storage for the browser is always owned by the current BVC, so it
    // must be updated when switching between BVCs.
    [_storageSwitcher
        changeStorageFromBrowserState:self.currentInterface.browserState
                       toBrowserState:interface.browserState];
  }

  _currentInterface = interface;

  // The internal state of the Handoff Manager depends on the current BVC.
  [self updateDeviceSharingManager];
}

- (id<BrowserInterface>)mainInterface {
  if (!_mainInterface) {
    // The backing coordinator should not have been created yet.
    DCHECK(!_mainBrowserCoordinator);
    _mainBrowserCoordinator =
        [self coordinatorForBrowserState:_browserState
                                tabModel:self.mainTabModel];
    [_mainBrowserCoordinator start];
    DCHECK(_mainBrowserCoordinator.viewController);
    _mainInterface =
        [[WrangledBrowser alloc] initWithCoordinator:_mainBrowserCoordinator];
  }
  return _mainInterface;
}

- (id<BrowserInterface>)incognitoInterface {
  if (!_incognitoInterface) {
    // The backing coordinator should not have been created yet.
    DCHECK(!_incognitoBrowserCoordinator);
    ios::ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    _incognitoBrowserCoordinator =
        [self coordinatorForBrowserState:otrBrowserState
                                tabModel:self.otrTabModel];
    [_incognitoBrowserCoordinator start];
    DCHECK(_incognitoBrowserCoordinator.viewController);
    _incognitoInterface = [[WrangledBrowser alloc]
        initWithCoordinator:_incognitoBrowserCoordinator];
  }
  return _incognitoInterface;
}

- (TabModel*)mainTabModel {
  DCHECK(_mainTabModel)
      << "-createMainTabModel must be called before -mainTabModel is accessed.";
  return _mainTabModel;
}

- (void)setMainTabModel:(TabModel*)mainTabModel {
  if (_mainTabModel == mainTabModel)
    return;

  if (_mainTabModel) {
    breakpad::StopMonitoringTabStateForTabModel(_mainTabModel);
    breakpad::StopMonitoringURLsForTabModel(_mainTabModel);
    [_mainTabModel browserStateDestroyed];
    if (_tabModelObserver) {
      [_mainTabModel removeObserver:_tabModelObserver];
    }
    [_mainTabModel removeObserver:self];
  }

  _mainTabModel = mainTabModel;
}

- (TabModel*)otrTabModel {
  if (!_otrTabModel) {
    self.otrTabModel = [self buildOtrTabModel:NO];
  }
  return _otrTabModel;
}

- (void)setOtrTabModel:(TabModel*)otrTabModel {
  if (_otrTabModel == otrTabModel)
    return;

  if (_otrTabModel) {
    breakpad::StopMonitoringTabStateForTabModel(_otrTabModel);
    [_otrTabModel browserStateDestroyed];
    if (_tabModelObserver) {
      [_otrTabModel removeObserver:_tabModelObserver];
    }
    [_otrTabModel removeObserver:self];
  }

  _otrTabModel = otrTabModel;
}

#pragma mark - BrowserViewInformation methods

- (void)haltAllTabs {
  [self.mainTabModel haltAllTabs];
  [self.otrTabModel haltAllTabs];
}

- (void)cleanDeviceSharingManager {
  [self.deviceSharingManager updateBrowserState:NULL];
}

#pragma mark - TabModelObserver

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  [self updateDeviceSharingManager];
}

- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab {
  [self updateDeviceSharingManager];
}

#pragma mark - Other public methods

- (void)updateDeviceSharingManager {
  if (!self.deviceSharingManager) {
    self.deviceSharingManager = [[DeviceSharingManager alloc] init];
  }
  [self.deviceSharingManager updateBrowserState:_browserState];

  GURL activeURL;
  Tab* currentTab = self.currentInterface.tabModel.currentTab;
  // Set the active URL if there's a current tab and the current BVC is not OTR.
  if (currentTab.webState && !self.currentInterface.incognito) {
    activeURL = currentTab.webState->GetVisibleURL();
  }
  [self.deviceSharingManager updateActiveURL:activeURL];
}

- (void)destroyAndRebuildIncognitoTabModel {
  // It is theoretically possible that a Tab has been added to |_otrTabModel|
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(![_otrTabModel count]);
  DCHECK(_browserState);

  // Stop watching the OTR tab model's state for crashes.
  breakpad::StopMonitoringTabStateForTabModel(self.otrTabModel);

  // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
  // constructed by calling the property getter.
  BOOL otrBVCIsCurrent = self.currentInterface == self.incognitoInterface;
  @autoreleasepool {
    // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
    // constructed by calling the property getter.
    [_incognitoBrowserCoordinator stop];
    _incognitoBrowserCoordinator = nil;
    _incognitoInterface = nil;

    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    self.otrTabModel = nil;
    if (otrBVCIsCurrent) {
      _currentInterface = nil;
    }
  }

  _browserState->DestroyOffTheRecordChromeBrowserState();

  // An empty _otrTabModel must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  self.otrTabModel = [self buildOtrTabModel:YES];
  DCHECK(![self.otrTabModel count]);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  if (otrBVCIsCurrent) {
    self.currentInterface = self.incognitoInterface;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  // Disconnect the DeviceSharingManager.
  [self cleanDeviceSharingManager];

  // At this stage, new BrowserCoordinators shouldn't be lazily constructed by
  // calling their property getters.
  [_mainBrowserCoordinator stop];
  _mainBrowserCoordinator = nil;
  [_incognitoBrowserCoordinator stop];
  _incognitoBrowserCoordinator = nil;

  [_mainTabModel closeAllTabs];
  [_otrTabModel closeAllTabs];
  // Handles removing observers and stopping breakpad monitoring.
  [self setMainTabModel:nil];
  [self setOtrTabModel:nil];

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (TabModel*)buildOtrTabModel:(BOOL)empty {
  DCHECK(_browserState);
  // Ensure that the OTR ChromeBrowserState is created.
  ios::ChromeBrowserState* otrBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);
  return [self tabModelForBrowserState:otrBrowserState empty:empty];
}

- (TabModel*)tabModelForBrowserState:(ios::ChromeBrowserState*)browserState
                               empty:(BOOL)empty {
  SessionWindowIOS* sessionWindow = nil;
  if (!empty) {
    // Load existing saved tab model state.
    NSString* statePath =
        base::SysUTF8ToNSString(browserState->GetStatePath().AsUTF8Unsafe());
    SessionIOS* session =
        [[SessionServiceIOS sharedService] loadSessionFromDirectory:statePath];
    if (session) {
      DCHECK_EQ(session.sessionWindows.count, 1u);
      sessionWindow = session.sessionWindows[0];
    }
  }

  // Create tab model from saved session (nil is ok).
  TabModel* tabModel =
      [[TabModel alloc] initWithSessionWindow:sessionWindow
                               sessionService:[SessionServiceIOS sharedService]
                                 browserState:browserState];
  // Add observers.
  if (_tabModelObserver) {
    [tabModel addObserver:_tabModelObserver];
    [tabModel addObserver:self];
  }
  breakpad::MonitorTabStateForTabModel(tabModel);

  return tabModel;
}

- (BrowserCoordinator*)coordinatorForBrowserState:
                           (ios::ChromeBrowserState*)browserState
                                         tabModel:(TabModel*)tabModel {
  BrowserCoordinator* coordinator =
      [[BrowserCoordinator alloc] initWithBaseViewController:nil
                                                browserState:browserState];
  coordinator.tabModel = tabModel;
  coordinator.applicationCommandHandler = _applicationCommandEndpoint;
  return coordinator;
}

@end
