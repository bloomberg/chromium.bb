// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#include "base/mac/objc_property_releaser.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/browser/physical_web/start_physical_web_discovery.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

@interface BrowserViewWrangler ()<TabModelObserver> {
  ios::ChromeBrowserState* _browserState;
  __unsafe_unretained id<TabModelObserver> _tabModelObserver;

  base::mac::ObjCPropertyReleaser _propertyReleaser_BrowserViewWrangler;
}

// Responsible for maintaining all state related to sharing to other devices.
// Redeclared readwrite from the readonly declaration in the Testing interface.
@property(nonatomic, retain, readwrite)
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

// Creates the correct BrowserViewController for the corresponding browser state
// and tab model.
- (BrowserViewController*)bvcForBrowserState:
                              (ios::ChromeBrowserState*)browserState
                                    tabModel:(TabModel*)tabModel;
@end

@implementation BrowserViewWrangler

// Properties defined in the BrowserViewInformation protocol.
@synthesize mainBVC = _mainBVC;
@synthesize mainTabModel = _mainTabModel;
@synthesize otrBVC = _otrBVC;
@synthesize otrTabModel = _otrTabModel;
@synthesize currentBVC = _currentBVC;
// Private properies.
@synthesize deviceSharingManager = _deviceSharingManager;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                    tabModelObserver:(id<TabModelObserver>)tabModelObserver {
  if ((self = [super init])) {
    _propertyReleaser_BrowserViewWrangler.Init(self,
                                               [BrowserViewWrangler class]);
    _browserState = browserState;
    _tabModelObserver = tabModelObserver;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  if (_tabModelObserver) {
    [_mainTabModel removeObserver:_tabModelObserver];
    [_otrTabModel removeObserver:_tabModelObserver];
  }
  [_mainTabModel removeObserver:self];
  [_otrTabModel removeObserver:self];

  // Stop URL monitoring of the main tab model.
  ios_internal::breakpad::StopMonitoringURLsForTabModel(_mainTabModel);

  // Stop Breakpad state monitoring of both tab models (if necessary).
  ios_internal::breakpad::StopMonitoringTabStateForTabModel(_mainTabModel);
  ios_internal::breakpad::StopMonitoringTabStateForTabModel(_otrTabModel);

  // Normally other objects will take care of unhooking the tab models from
  // the browser state, but this code should ensure that it happens regardless.
  [_mainTabModel browserStateDestroyed];
  [_otrTabModel browserStateDestroyed];

  [super dealloc];
}

#pragma mark - BrowserViewInformation property implementations

- (BrowserViewController*)mainBVC {
  if (!_mainBVC) {
    // |_browserState| should always be set before trying to create
    // |_mainBVC|.
    DCHECK(_browserState);
    self.mainBVC =
        [self bvcForBrowserState:_browserState tabModel:self.mainTabModel];
    DCHECK(_mainBVC);
  }
  return _mainBVC;
}

- (TabModel*)mainTabModel {
  if (!_mainTabModel) {
    self.mainTabModel = [self tabModelForBrowserState:_browserState empty:NO];
    // Follow loaded URLs in the main tab model to send those in case of
    // crashes.
    ios_internal::breakpad::MonitorURLsForTabModel(_mainTabModel);
    ios::GetChromeBrowserProvider()->InitializeCastService(_mainTabModel);
  }
  return _mainTabModel;
}

- (BrowserViewController*)otrBVC {
  if (!_otrBVC) {
    // |_browserState| should always be set before trying to create
    // |_otrBVC|.
    DCHECK(_browserState);
    ios::ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    self.otrBVC =
        [self bvcForBrowserState:otrBrowserState tabModel:self.otrTabModel];
    DCHECK(_otrBVC);
  }
  return _otrBVC;
}

- (TabModel*)otrTabModel {
  if (!_otrTabModel) {
    self.otrTabModel = [self buildOtrTabModel:NO];
  }
  return _otrTabModel;
}

- (void)setCurrentBVC:(BrowserViewController*)bvc
      storageSwitcher:(id<BrowserStateStorageSwitching>)storageSwitcher {
  DCHECK(bvc != nil);
  // |bvc| should be one of the BrowserViewControllers this class already owns.
  DCHECK(_mainBVC == bvc || _otrBVC == bvc);
  if (self.currentBVC == bvc) {
    return;
  }

  if (self.currentBVC) {
    // Tell the current BVC it moved to the background.
    [self.currentBVC setPrimary:NO];

    // Data storage for the browser is always owned by the current BVC, so it
    // must be updated when switching between BVCs.
    [storageSwitcher changeStorageFromBrowserState:self.currentBVC.browserState
                                    toBrowserState:bvc.browserState];
  }

  self.currentBVC = bvc;

  // The internal state of the Handoff Manager depends on the current BVC.
  [self updateDeviceSharingManager];

  // By default, Physical Web discovery will not be started if the browser
  // launches into an Incognito tab. On switching modes, check if discovery
  // should be started.
  StartPhysicalWebDiscovery(GetApplicationContext()->GetLocalState(),
                            [self currentBrowserState]);
}

#pragma mark - BrowserViewInformation methods

- (TabModel*)currentTabModel {
  return self.currentBVC.tabModel;
}

- (ios::ChromeBrowserState*)currentBrowserState {
  return self.currentBVC.browserState;
}

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
    self.deviceSharingManager =
        [[[DeviceSharingManager alloc] init] autorelease];
  }
  [self.deviceSharingManager updateBrowserState:_browserState];

  GURL activeURL;
  Tab* currentTab = [self.currentBVC tabModel].currentTab;
  // Set the active URL if there's a current tab and the current BVC is not OTR.
  if (currentTab && self.currentBVC != self.otrBVC) {
    activeURL = currentTab.url;
  }
  [self.deviceSharingManager updateActiveURL:activeURL];
}

- (void)deleteIncognitoTabModelState:
    (BrowsingDataRemovalController*)removalController {
  // It is theoretically possible that a Tab has been added to |_otrTabModel|
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(![_otrTabModel count]);
  DCHECK(_browserState);

  // Stop watching the OTR tab model's state for crashes.
  ios_internal::breakpad::StopMonitoringTabStateForTabModel(self.otrTabModel);

  // At this stage, a new OTR BVC shouldn't be lazily constructed by calling the
  // .otrBVC property getter. Instead, the ivar is accessed directly through the
  // following code.
  BOOL otrBVCIsCurrent = self.currentBVC == _otrBVC;
  @autoreleasepool {
    ios::ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    [removalController browserStateDestroyed:otrBrowserState];
    [_otrBVC browserStateDestroyed];
    [_otrBVC release];
    _otrBVC = nil;
    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    [_otrTabModel browserStateDestroyed];
    if (_tabModelObserver) {
      [_otrTabModel removeObserver:_tabModelObserver];
    }
    [_otrTabModel removeObserver:self];
    [_otrTabModel release];
    _otrTabModel = nil;
    if (otrBVCIsCurrent) {
      _currentBVC = nil;
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
    _currentBVC = self.otrBVC;
  }
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
    sessionWindow = [[SessionServiceIOS sharedService]
        loadWindowForBrowserState:browserState];
  }

  // Create tab model from saved session (nil is ok).
  TabModel* tabModel =
      [[[TabModel alloc] initWithSessionWindow:sessionWindow
                                sessionService:[SessionServiceIOS sharedService]
                                  browserState:browserState] autorelease];
  // Add observers.
  if (_tabModelObserver) {
    [tabModel addObserver:_tabModelObserver];
    [tabModel addObserver:self];
  }
  ios_internal::breakpad::MonitorTabStateForTabModel(tabModel);

  return tabModel;
}

- (BrowserViewController*)bvcForBrowserState:
                              (ios::ChromeBrowserState*)browserState
                                    tabModel:(TabModel*)tabModel {
  base::scoped_nsobject<BrowserViewControllerDependencyFactory> factory(
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowserState:browserState]);
  return [[[BrowserViewController alloc] initWithTabModel:tabModel
                                             browserState:browserState
                                        dependencyFactory:factory] autorelease];
}

@end
