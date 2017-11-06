// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab.h"

#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/ios/block_types.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/google/core/browser/google_util.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/crash_loop_detection_util.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_tab_helper.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#include "ios/chrome/browser/signin/signin_capability.h"
#import "ios/chrome/browser/snapshots/snapshot_manager.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay_provider.h"
#import "ios/chrome/browser/snapshots/web_controller_snapshot_helper.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_delegate.h"
#import "ios/chrome/browser/tabs/tab_dialog_delegate.h"
#import "ios/chrome/browser/tabs/tab_headers_delegate.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/tabs/tab_snapshotting_delegate.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/u2f/u2f_controller.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/downloads/download_manager_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/legacy_fullscreen_controller.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/external_app_launcher.h"
#import "ios/chrome/browser/web/navigation_manager_util.h"
#import "ios/chrome/browser/web/passkit_dialog_provider.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/favicon_url.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kTabUrlStartedLoadingNotificationForCrashReporting =
    @"kTabUrlStartedLoadingNotificationForCrashReporting";
NSString* const kTabUrlMayStartLoadingNotificationForCrashReporting =
    @"kTabUrlMayStartLoadingNotificationForCrashReporting";
NSString* const kTabIsShowingExportableNotificationForCrashReporting =
    @"kTabIsShowingExportableNotificationForCrashReporting";
NSString* const kTabClosingCurrentDocumentNotificationForCrashReporting =
    @"kTabClosingCurrentDocumentNotificationForCrashReporting";

NSString* const kTabUrlKey = @"url";

namespace {
class TabInfoBarObserver;

// Returns true if |item| is the result of a HTTP redirect.
// Returns false if |item| is nullptr;
bool IsItemRedirectItem(web::NavigationItem* item) {
  if (!item)
    return false;

  return (ui::PageTransition::PAGE_TRANSITION_IS_REDIRECT_MASK &
          item->GetTransitionType()) == 0;
}
}  // namespace

@interface Tab ()<CRWWebStateObserver,
                  CRWWebControllerObserver,
                  FindInPageControllerDelegate> {
  __weak TabModel* _parentTabModel;
  ios::ChromeBrowserState* _browserState;

  OpenInController* _openInController;


  // YES if this Tab is being prerendered.
  BOOL _isPrerenderTab;

  // Last visited timestamp.
  double _lastVisitedTimestamp;

  // The Full Screen Controller responsible for hiding/showing the toolbar.
  LegacyFullscreenController* _legacyFullscreenController;

  // The Overscroll controller responsible for displaying the
  // overscrollActionsView above the toolbar.
  OverscrollActionsController* _overscrollActionsController;

  // Lightweight object dealing with various different UI behaviours when
  // opening a URL in an external application.
  ExternalAppLauncher* _externalAppLauncher;

  // Handles retrieving, generating and updating snapshots of CRWWebController's
  // web page.
  WebControllerSnapshotHelper* _webControllerSnapshotHelper;

  // WebStateImpl for this tab.
  web::WebStateImpl* _webStateImpl;

  // Allows Tab to conform CRWWebStateDelegate protocol.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

  // Universal Second Factor (U2F) call controller.
  U2FController* _secondFactorController;

  // C++ observer used to trigger snapshots after the removal of InfoBars.
  std::unique_ptr<TabInfoBarObserver> _tabInfoBarObserver;

  // View displayed upon PagePlaceholderTabHelperDelegate request.
  UIImageView* _pagePlaceholder;
}

// Handles caching and retrieving of snapshots.
@property(nonatomic, strong) SnapshotManager* snapshotManager;

// Returns the OpenInController for this tab.
- (OpenInController*)openInController;

// Handles exportable files if possible.
- (void)handleExportableFile:(net::HttpResponseHeaders*)headers;

// Returns YES if TabUsageRecorder::RecordPageLoadStart should be called for the
// given navigation.
- (BOOL)shouldRecordPageLoadStartForNavigation:
    (web::NavigationContext*)navigation;

@end

namespace {
// Observer class that listens for infobar signals.
class TabInfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  TabInfoBarObserver(Tab* owner, infobars::InfoBarManager* infobar_manager);
  ~TabInfoBarObserver() override;
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

 private:
  __weak Tab* owner_;
  ScopedObserver<infobars::InfoBarManager, TabInfoBarObserver> scoped_observer_;
  DISALLOW_COPY_AND_ASSIGN(TabInfoBarObserver);
};

TabInfoBarObserver::TabInfoBarObserver(
    Tab* owner,
    infobars::InfoBarManager* infobar_manager)
    : owner_(owner), scoped_observer_(this) {
  DCHECK(infobar_manager);
  scoped_observer_.Add(infobar_manager);
}

TabInfoBarObserver::~TabInfoBarObserver() {}

void TabInfoBarObserver::OnInfoBarAdded(infobars::InfoBar* infobar) {
  // Update snapshots after the infobar has been added.
  [owner_ updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
}

void TabInfoBarObserver::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                          bool animate) {
  // Update snapshots after the infobar has been removed.
  [owner_ updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
}

void TabInfoBarObserver::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                           infobars::InfoBar* new_infobar) {
  // Update snapshots after the infobar has been replaced.
  [owner_ updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
}

}  // anonymous namespace

@implementation Tab

@synthesize browserState = _browserState;
@synthesize useGreyImageCache = useGreyImageCache_;
@synthesize isPrerenderTab = _isPrerenderTab;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollActionsControllerDelegate =
    overscrollActionsControllerDelegate_;
@synthesize passKitDialogProvider = passKitDialogProvider_;
@synthesize delegate = delegate_;
@synthesize dialogDelegate = dialogDelegate_;
@synthesize snapshotOverlayProvider = snapshotOverlayProvider_;
@synthesize tabSnapshottingDelegate = tabSnapshottingDelegate_;
@synthesize tabHeadersDelegate = tabHeadersDelegate_;
@synthesize legacyFullscreenControllerDelegate =
    legacyFullscreenControllerDelegate_;
@synthesize snapshotManager = _snapshotManager;

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super init];
  if (self) {
    // TODO(crbug.com/620465): Tab should only use public API of WebState.
    // Remove this cast once this is the case.
    _webStateImpl = static_cast<web::WebStateImpl*>(webState);
    _webStateObserver =
        base::MakeUnique<web::WebStateObserverBridge>(webState, self);
    _browserState =
        ios::ChromeBrowserState::FromBrowserState(webState->GetBrowserState());

    [self updateLastVisitedTimestamp];
    [[self webController] addObserver:self];
    [[self webController] setDelegate:self];

    _snapshotManager = [[SnapshotManager alloc] initWithWebState:webState];
    _webControllerSnapshotHelper = [[WebControllerSnapshotHelper alloc]
        initWithSnapshotManager:_snapshotManager
                            tab:self];
  }
  return self;
}

- (void)attachTabHelpers {
  _tabInfoBarObserver = std::make_unique<TabInfoBarObserver>(
      self, InfoBarManagerImpl::FromWebState(self.webState));
}

- (id<FindInPageControllerDelegate>)findInPageControllerDelegate {
  return self;
}

- (void)setParentTabModel:(TabModel*)model {
  DCHECK(!model || !_parentTabModel);
  _parentTabModel = model;

  if (_parentTabModel.syncedWindowDelegate) {
    IOSChromeSessionTabHelper::FromWebState(self.webState)
        ->SetWindowID(model.sessionID);
  }
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"%p ... %@ - %s", self, self.title,
                                 self.webState->GetVisibleURL().spec().c_str()];
}

- (id<TabDialogDelegate>)dialogDelegate {
  return dialogDelegate_;
}

- (BOOL)loadFinished {
  return self.webState && !self.webState->IsLoading();
}

- (BOOL)isVoiceSearchResultsTab {
  // TODO(crbug.com/778416): Move this logic entirely into helper.
  // If nothing has been loaded in the Tab, it cannot be displaying a voice
  // search results page.
  web::NavigationItem* item =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!item)
    return NO;
  // Navigating through history to a NavigationItem that was created for a voice
  // search query should just be treated like a normal page load.
  if ((item->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) != 0)
    return NO;
  // Check whether |item| has been marked as a voice search result navigation.
  return VoiceSearchNavigationTabHelper::FromWebState(self.webState)
      ->IsNavigationFromVoiceSearch(item);
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  [_webControllerSnapshotHelper
      retrieveSnapshotForWebController:self.webController
                             sessionID:self.tabId
                          withOverlays:[self snapshotOverlays]
                              callback:callback];
}

- (NSString*)title {
  base::string16 title = self.webState->GetTitle();
  if (title.empty())
    title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  return base::SysUTF16ToNSString(title);
}

- (NSString*)urlDisplayString {
  base::string16 urlText = url_formatter::FormatUrl(
      self.webState->GetVisibleURL(), url_formatter::kFormatUrlOmitNothing,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  return base::SysUTF16ToNSString(urlText);
}

- (NSString*)tabId {
  if (!self.webState) {
    // Tab can outlive WebState, in which case Tab is not valid anymore and
    // tabId should be nil.
    return nil;
  }
  TabIdTabHelper* tab_id_helper = TabIdTabHelper::FromWebState(self.webState);
  DCHECK(tab_id_helper);
  return tab_id_helper->tab_id();
}

- (web::WebState*)webState {
  return _webStateImpl;
}

- (UIView*)view {
  if (!self.webState)
    return nil;

  // Record reload of previously-evicted tab.
  if (self.webState->IsEvicted() && [_parentTabModel tabUsageRecorder])
    [_parentTabModel tabUsageRecorder]->RecordPageLoadStart(self.webState);

  // Do not trigger the load if the tab has crashed. SadTabTabHelper is
  // responsible for handing reload logic for crashed tabs.
  if (!self.webState->IsCrashed()) {
    self.webState->GetNavigationManager()->LoadIfNecessary();
  }
  return self.webState->GetView();
}

- (UIView*)viewForPrinting {
  return self.webController.viewForPrinting;
}

- (web::NavigationManager*)navigationManager {
  return self.webState ? self.webState->GetNavigationManager() : nullptr;
}

- (void)setIsPrerenderTab:(BOOL)isPrerender {
  if (_isPrerenderTab == isPrerender)
    return;

  _isPrerenderTab = isPrerender;

  self.webState->SetShouldSuppressDialogs(isPrerender);

  if (_isPrerenderTab)
    return;

  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    [_legacyFullscreenController moveContentBelowHeader];
  }

  // If the page has finished loading, take a snapshot.  If the page is still
  // loading, do nothing, as CRWWebController will automatically take a
  // snapshot once the load completes.
  if ([self loadFinished])
    [self updateSnapshotWithOverlay:YES visibleFrameOnly:YES];

  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForTab:self
               loadSuccess:[self loadFinished]];
  [self countMainFrameLoad];
}

- (void)setLegacyFullscreenControllerDelegate:
    (id<LegacyFullscreenControllerDelegate>)fullScreenControllerDelegate {
  DCHECK(!base::FeatureList::IsEnabled(features::kNewFullscreen));
  if (fullScreenControllerDelegate == legacyFullscreenControllerDelegate_)
    return;
  // Lazily create a LegacyFullscreenController.
  // The check for fullScreenControllerDelegate is necessary to avoid recreating
  // a LegacyFullscreenController during teardown.
  if (!_legacyFullscreenController && fullScreenControllerDelegate) {
    _legacyFullscreenController = [[LegacyFullscreenController alloc]
         initWithDelegate:fullScreenControllerDelegate
        navigationManager:self.navigationManager
                sessionID:self.tabId];
    [self.webController addObserver:_legacyFullscreenController];
    // If the content of the page was loaded without knowledge of the
    // toolbar position it will be misplaced under the toolbar instead of
    // right below. This happens e.g. in the case of preloading. This is to make
    // sure the content is moved to the right place.
    [_legacyFullscreenController moveContentBelowHeader];
  }
  legacyFullscreenControllerDelegate_ = fullScreenControllerDelegate;
}

- (void)setOverscrollActionsControllerDelegate:
    (id<OverscrollActionsControllerDelegate>)
        overscrollActionsControllerDelegate {
  if (overscrollActionsControllerDelegate_ ==
      overscrollActionsControllerDelegate) {
    return;
  }

  // Lazily create a OverscrollActionsController.
  // The check for overscrollActionsControllerDelegate is necessary to avoid
  // recreating a OverscrollActionsController during teardown.
  if (!_overscrollActionsController) {
    _overscrollActionsController = [[OverscrollActionsController alloc] init];
    [self.webController addObserver:_overscrollActionsController];
  }
  OverscrollStyle style = OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  if (_browserState->IsOffTheRecord())
    style = OverscrollStyle::REGULAR_PAGE_INCOGNITO;
  [_overscrollActionsController setStyle:style];
  [_overscrollActionsController
      setDelegate:overscrollActionsControllerDelegate];
  overscrollActionsControllerDelegate_ = overscrollActionsControllerDelegate;
}

- (void)webDidUpdateSessionForLoadWithURL:(const GURL&)URL {
  // After a crash the NTP is loaded by default.
  if (URL.host() != kChromeUINewTabHost) {
    static BOOL hasLoadedPage = NO;
    if (!hasLoadedPage) {
      // As soon as load is initialted, a crash shouldn't be counted as a
      // startup crash. Since initiating a url load requires user action and is
      // a significant source of crashes that could lead to false positives in
      // crash loop detection.
      crash_util::ResetFailedStartupAttemptCount();
      hasLoadedPage = YES;
    }
  }

  web::NavigationItem* navigationItem =
      [self navigationManager]->GetPendingItem();

  // TODO(crbug.com/676129): the pending item is not correctly set when the
  // page is reloading, use the last committed item if pending item is null.
  // Remove this once tracking bug is fixed.
  if (!navigationItem)
    navigationItem = [self navigationManager]->GetLastCommittedItem();

  [[OmniboxGeolocationController sharedInstance]
      addLocationToNavigationItem:navigationItem
                     browserState:_browserState];
}

// Halt the tab, which amounts to halting its webController.
- (void)terminateNetworkActivity {
  [self.webController terminateNetworkActivity];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webStateImpl, webState);
  self.overscrollActionsControllerDelegate = nil;
  self.passKitDialogProvider = nil;
  self.snapshotOverlayProvider = nil;

  _tabInfoBarObserver.reset();

  [_openInController detachFromWebController];
  _openInController = nil;
  if (_overscrollActionsController)
    [self.webController removeObserver:_overscrollActionsController];
  [_overscrollActionsController invalidate];
  _overscrollActionsController = nil;

  // Clean up legacy fullscreen.
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    self.legacyFullscreenControllerDelegate = nil;
    if (_legacyFullscreenController)
      [self.webController removeObserver:_legacyFullscreenController];
    [_legacyFullscreenController invalidate];
    _legacyFullscreenController = nil;
  }

  // Cancel any queued dialogs.
  [self.dialogDelegate cancelDialogForTab:self];

  _webStateObserver.reset();
  _webStateImpl = nullptr;
}

- (void)dismissModals {
  [_openInController disable];
  [self.webController dismissModals];
}

- (void)goBack {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoBack());
    base::RecordAction(base::UserMetricsAction("Back"));
    self.navigationManager->GoBack();
  }
}

- (void)goForward {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoForward());
    base::RecordAction(base::UserMetricsAction("Forward"));
    self.navigationManager->GoForward();
  }
}

- (BOOL)canGoBack {
  return self.navigationManager && self.navigationManager->CanGoBack();
}

- (BOOL)canGoForward {
  return self.navigationManager && self.navigationManager->CanGoForward();
}

- (void)goToItem:(const web::NavigationItem*)item {
  DCHECK(item);
  int index = self.navigationManager->GetIndexOfItem(item);
  DCHECK_NE(index, -1);
  self.navigationManager->GoToIndex(index);
}

- (BOOL)openExternalURL:(const GURL&)url
              sourceURL:(const GURL&)sourceURL
            linkClicked:(BOOL)linkClicked {
  if (!_externalAppLauncher)
    _externalAppLauncher = [[ExternalAppLauncher alloc] init];

  // Make a local url copy for possible modification.
  GURL finalURL = url;

  // Check if it's a direct FIDO U2F x-callback call. If so, do not open it, to
  // prevent pages from spoofing requests with different origins.
  if (finalURL.SchemeIs("u2f-x-callback"))
    return NO;

  // Block attempts to open this application's settings in the native system
  // settings application.
  if (finalURL.SchemeIs("app-settings"))
    return NO;

  // Check if it's a FIDO U2F call.
  if (finalURL.SchemeIs("u2f")) {
    // Create U2FController object lazily.
    if (!_secondFactorController)
      _secondFactorController = [[U2FController alloc] init];

    DCHECK([self navigationManager]);
    GURL origin =
        [self navigationManager]->GetLastCommittedItem()->GetURL().GetOrigin();

    // Compose u2f-x-callback URL and update urlToOpen.
    finalURL = [_secondFactorController
        XCallbackFromRequestURL:finalURL
                      originURL:origin
                         tabURL:self.webState->GetLastCommittedURL()
                          tabID:self.tabId];

    if (!finalURL.is_valid())
      return NO;
  }

  if ([_externalAppLauncher requestToOpenURL:finalURL
                               sourcePageURL:sourceURL
                                 linkClicked:linkClicked]) {
    // Clears pending navigation history after successfully launching the
    // external app.
    DCHECK([self navigationManager]);
    [self navigationManager]->DiscardNonCommittedItems();
    // Ensure the UI reflects the current entry, not the just-discarded pending
    // entry.
    [_parentTabModel notifyTabChanged:self];

    if (sourceURL.is_valid()) {
      ReadingListModel* model =
          ReadingListModelFactory::GetForBrowserState(_browserState);
      if (model && model->loaded())
        model->SetReadStatus(sourceURL, true);
    }

    return YES;
  }
  return NO;
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (!navigation->GetError()) {
    [self countMainFrameLoad];
  }

  [_parentTabModel notifyTabChanged:self];
}

// Records metric for the interface's orientation.
- (void)recordInterfaceOrientation {
  switch ([[UIApplication sharedApplication] statusBarOrientation]) {
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationPortraitUpsideDown:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", YES);
      break;
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", NO);
      break;
    case UIInterfaceOrientationUnknown:
      // TODO(crbug.com/228832): Convert from a boolean histogram to an
      // enumerated histogram and log this case as well.
      break;
  }
}

- (OpenInController*)openInController {
  if (!_openInController) {
    _openInController = [[OpenInController alloc]
        initWithRequestContext:_browserState->GetRequestContext()
                 webController:self.webController];
  }
  return _openInController;
}

- (id<CRWNativeContent>)controllerForUnhandledContentAtURL:(const GURL&)url {
  // Shows download manager UI for unhandled content.
  DownloadManagerController* downloadController =
      [[DownloadManagerController alloc] initWithWebState:self.webState
                                              downloadURL:url];
  [downloadController start];
  return downloadController;
}

- (void)handleExportableFile:(net::HttpResponseHeaders*)headers {
  // Only "application/pdf" is supported for now.
  if (self.webState->GetContentsMimeType() != "application/pdf")
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabIsShowingExportableNotificationForCrashReporting
                    object:self];
  // Try to generate a filename by first looking at |content_disposition_|, then
  // at the last component of WebState's last committed URL and if both of these
  // fail use the default filename "document".
  std::string contentDisposition;
  if (headers)
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  std::string defaultFilename =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  const GURL& lastCommittedURL = self.webState->GetLastCommittedURL();
  base::string16 filename =
      net::GetSuggestedFilename(lastCommittedURL, contentDisposition,
                                "",                 // referrer-charset
                                "",                 // suggested-name
                                "application/pdf",  // mime-type
                                defaultFilename);
  [[self openInController]
      enableWithDocumentURL:lastCommittedURL
          suggestedFilename:base::SysUTF16ToNSString(filename)];
}

- (void)countMainFrameLoad {
  if ([self isPrerenderTab] ||
      self.webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme)) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobilePageLoaded"));
}

- (BOOL)shouldRecordPageLoadStartForNavigation:
    (web::NavigationContext*)navigation {
  web::NavigationItem* lastCommittedItem =
      [self navigationManager]->GetLastCommittedItem();
  if (!lastCommittedItem) {
    // Opening a child window and loading URL there (crbug.com/773160).
    return NO;
  }

  web::NavigationItem* pendingItem = [self navigationManager]->GetPendingItem();
  if (pendingItem) {
    using web::UserAgentType;
    UserAgentType committedUserAgent = lastCommittedItem->GetUserAgentType();
    UserAgentType pendingUserAgent = pendingItem->GetUserAgentType();
    if (committedUserAgent != web::UserAgentType::NONE &&
        pendingUserAgent != web::UserAgentType::NONE &&
        committedUserAgent != pendingUserAgent) {
      // Switching to Desktop or Mobile User Agent.
      return YES;
    }
  }

  ui::PageTransition transition = navigation->GetPageTransition();
  if (!ui::PageTransitionIsNewNavigation(transition)) {
    // Back forward navigation or reload.
    return NO;
  }

  if ((ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT & transition) != 0) {
    // Client redirect.
    return NO;
  }

  using ui::PageTransitionCoreTypeIs;
  return (
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_FORM_SUBMIT) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_KEYWORD) ||
      PageTransitionCoreTypeIs(transition,
                               ui::PAGE_TRANSITION_KEYWORD_GENERATED));
}

#pragma mark -
#pragma mark FindInPageControllerDelegate

- (void)willAdjustScrollPosition {
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    // Skip the next attempt to correct the scroll offset for the toolbar
    // height.  Used when programatically scrolling down the y offset.
    [_legacyFullscreenController shouldSkipNextScrollOffsetForHeader];
  }
}

#pragma mark -
#pragma mark FullScreen

- (void)updateFullscreenWithToolbarVisible:(BOOL)visible {
  DCHECK(!base::FeatureList::IsEnabled(features::kNewFullscreen));
  [_legacyFullscreenController moveHeaderToRestingPosition:visible];
}

#pragma mark -

- (BOOL)usesDesktopUserAgent {
  if (!self.navigationManager)
    return NO;

  web::NavigationItem* visibleItem = self.navigationManager->GetVisibleItem();
  return visibleItem &&
         visibleItem->GetUserAgentType() == web::UserAgentType::DESKTOP;
}

- (void)reloadWithUserAgentType:(web::UserAgentType)userAgentType {
  // This removes the web view, which will be recreated at the end of this.
  [self.webController requirePageReconstruction];

  // TODO(crbug.com/228171): A hack in session_controller -addPendingItem
  // discusses making tab responsible for distinguishing history stack
  // navigation from new navigations.
  web::NavigationManager* navigationManager = [self navigationManager];
  DCHECK(navigationManager);

  web::NavigationItem* lastNonRedirectItem =
      navigationManager->GetTransientItem();
  if (!lastNonRedirectItem || IsItemRedirectItem(lastNonRedirectItem))
    lastNonRedirectItem = navigationManager->GetVisibleItem();
  if (!lastNonRedirectItem || IsItemRedirectItem(lastNonRedirectItem))
    lastNonRedirectItem = GetLastCommittedNonRedirectedItem(navigationManager);

  if (!lastNonRedirectItem)
    return;

  // |reloadURL| will be empty if a page was open by DOM.
  GURL reloadURL(lastNonRedirectItem->GetOriginalRequestURL());
  if (reloadURL.is_empty()) {
    DCHECK(self.webState && self.webState->HasOpener());
    reloadURL = lastNonRedirectItem->GetVirtualURL();
  }

  web::NavigationManager::WebLoadParams params(reloadURL);
  params.referrer = lastNonRedirectItem->GetReferrer();
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;

  switch (userAgentType) {
    case web::UserAgentType::DESKTOP:
      params.user_agent_override_option =
          web::NavigationManager::UserAgentOverrideOption::DESKTOP;
      break;
    case web::UserAgentType::MOBILE:
      params.user_agent_override_option =
          web::NavigationManager::UserAgentOverrideOption::MOBILE;
      break;
    case web::UserAgentType::NONE:
      NOTREACHED();
  }

  navigationManager->LoadURLWithParams(params);
}

- (void)evaluateU2FResultFromURL:(const GURL&)URL {
  DCHECK(_secondFactorController);
  [_secondFactorController evaluateU2FResultFromU2FURL:URL
                                              webState:self.webState];
}

#pragma mark - CRWWebControllerObserver protocol methods.

- (void)webControllerWillClose:(CRWWebController*)webController {
  DCHECK_EQ(webController, [self webController]);
  [[self webController] removeObserver:self];
  [[self webController] setDelegate:nil];
}

#pragma mark - CRWWebDelegate and CRWWebStateObserver protocol methods.

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  if (!navigation->IsSameDocument() &&
      !base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    // Move the toolbar to visible during page load.
    [_legacyFullscreenController disableFullScreen];
  }

  if ([self shouldRecordPageLoadStartForNavigation:navigation] &&
      [_parentTabModel tabUsageRecorder] && !_isPrerenderTab) {
    [_parentTabModel tabUsageRecorder]->RecordPageLoadStart(webState);
  }

  [self.dialogDelegate cancelDialogForTab:self];
  [_parentTabModel notifyTabChanged:self];
  [_openInController disable];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:
          kTabClosingCurrentDocumentNotificationForCrashReporting
                    object:self];
}

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  DCHECK([self navigationManager]);
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    // |webWillAddPendingURL:transition:| is not called for native page loads.
    // TODO(crbug.com/381201): Move this call there once that bug is fixed so
    // that |disableFullScreen| is called only from one place.
    [_legacyFullscreenController disableFullScreen];
  }
  GURL lastCommittedURL = webState->GetLastCommittedURL();

  if (_parentTabModel) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabWillStartLoadingNotification
                      object:_parentTabModel
                    userInfo:@{kTabModelTabKey : self}];
  }
  [_parentTabModel notifyTabChanged:self];
  if (_parentTabModel) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabDidStartLoadingNotification
                      object:_parentTabModel
                    userInfo:@{kTabModelTabKey : self}];
  }

  web::NavigationItem* previousItem = nullptr;
  if (details.previous_item_index >= 0) {
    previousItem = webState->GetNavigationManager()->GetItemAtIndex(
        details.previous_item_index);
  }

  [_parentTabModel navigationCommittedInTab:self previousItem:previousItem];

  // Sending a notification about the url change for crash reporting.
  // TODO(crbug.com/661675): Consider using the navigation entry committed
  // notification now that it's in the right place.
  NSString* URLSpec = base::SysUTF8ToNSString(lastCommittedURL.spec());
  if (URLSpec.length) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabUrlStartedLoadingNotificationForCrashReporting
                      object:self
                    userInfo:@{kTabUrlKey : URLSpec}];
  }
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  DCHECK([self loadFinished]);

  // Cancel prerendering if response is "application/octet-stream". It can be a
  // video file which should not be played from preload tab (crbug.com/436813).
  if (_isPrerenderTab &&
      self.webState->GetContentsMimeType() == "application/octet-stream") {
    [delegate_ discardPrerender];
  }

  bool wasPost = false;
  GURL lastCommittedURL;
  web::NavigationItem* lastCommittedItem =
      [self navigationManager]->GetLastCommittedItem();
  if (lastCommittedItem) {
    wasPost = lastCommittedItem->HasPostData();
    lastCommittedURL = lastCommittedItem->GetVirtualURL();
  }
  [_webControllerSnapshotHelper setSnapshotCoalescingEnabled:YES];
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen) && !loadSuccess)
    [_legacyFullscreenController disableFullScreen];
  [self recordInterfaceOrientation];
  navigation_metrics::RecordMainFrameNavigation(
      lastCommittedURL, true, self.browserState->IsOffTheRecord());

  if (loadSuccess) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        _webStateImpl->GetHttpResponseHeaders();
    [self handleExportableFile:headers.get()];
  }

  [_parentTabModel notifyTabChanged:self];

  if (_parentTabModel) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabDidFinishLoadingNotification
                      object:_parentTabModel
                    userInfo:[NSDictionary
                                 dictionaryWithObjectsAndKeys:
                                     self, kTabModelTabKey,
                                     [NSNumber numberWithBool:loadSuccess],
                                     kTabModelPageLoadSuccess, nil]];
  }
  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForTab:self
               loadSuccess:loadSuccess];

  if (loadSuccess)
    [self updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
  [_webControllerSnapshotHelper setSnapshotCoalescingEnabled:NO];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  // TODO(crbug.com/546406): It is probably possible to do something smarter,
  // but the fact that this is not always sent will have to be taken into
  // account.
  [_parentTabModel notifyTabChanged:self];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [_parentTabModel notifyTabChanged:self];
}

- (void)webStateDidDismissInterstitial:(web::WebState*)webState {
  [_parentTabModel notifyTabChanged:self];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    // This is the maximum that a page will ever load and it is safe to allow
    // fullscreen mode.
    [_legacyFullscreenController enableFullScreen];
  }
  [_parentTabModel notifyTabChanged:self];
}

- (BOOL)webController:(CRWWebController*)webController
    shouldOpenExternalURL:(const GURL&)URL {
  if (_isPrerenderTab) {
    [delegate_ discardPrerender];
    return NO;
  }
  return YES;
}

- (double)lastVisitedTimestamp {
  return _lastVisitedTimestamp;
}

- (void)updateLastVisitedTimestamp {
  _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
}

- (NSArray*)snapshotOverlays {
  return [snapshotOverlayProvider_ snapshotOverlaysForTab:self];
}

- (void)webViewRemoved {
  [_openInController disable];
}

- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL {
  // chrome:// URLs are only allowed if the mainDocumentURL is also a chrome://
  // URL.
  if (url.SchemeIs(kChromeUIScheme) &&
      !mainDocumentURL.SchemeIs(kChromeUIScheme)) {
    return NO;
  }

  // Always allow frame loads.
  BOOL isFrameLoad = (url != mainDocumentURL);
  if (isFrameLoad)
    return YES;

  // TODO(crbug.com/546402): If this turns out to be useful, find a less hacky
  // hook point to send this from.
  NSString* urlString = base::SysUTF8ToNSString(url.spec());
  if ([urlString length]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabUrlMayStartLoadingNotificationForCrashReporting
                      object:self
                    userInfo:@{kTabUrlKey : urlString}];
  }

  return YES;
}

- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block {
  [self getPlaceholderOverlayImageWithCompletionHandler:block];
}

#pragma mark - PlaceholderOverlay

- (void)getPlaceholderOverlayImageWithCompletionHandler:
    (void (^)(UIImage*))completionHandler {
  NSString* sessionID = self.tabId;
  // The snapshot is always grey, even if |useGreyImageCache_| is NO, as this
  // overlay represents an out-of-date website and is shown only until the
  // has begun loading. However, if |useGreyImageCache_| is YES, the grey image
  // is already cached in memory for swiping, and a cache miss is acceptable.
  // In other cases, such as during startup, either disk access or a greyspace
  // conversion is required, as there will be no grey snapshots in memory.
  if (useGreyImageCache_) {
    [self.snapshotManager greyImageForSessionID:sessionID
                                       callback:completionHandler];
  } else {
    [_webControllerSnapshotHelper
        retrieveGreySnapshotForWebController:self.webController
                                   sessionID:sessionID
                                withOverlays:[self snapshotOverlays]
                                    callback:completionHandler];
  }
}

- (UIImage*)updateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                     visibleFrameOnly:(BOOL)visibleFrameOnly {
  NSArray* overlays = shouldAddOverlay ? [self snapshotOverlays] : nil;
  UIImage* snapshot = [_webControllerSnapshotHelper
      updateSnapshotForWebController:self.webController
                           sessionID:self.tabId
                        withOverlays:overlays
                    visibleFrameOnly:visibleFrameOnly];
  [_parentTabModel notifyTabSnapshotChanged:self withImage:snapshot];
  return snapshot;
}

- (UIImage*)generateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  NSArray* overlays = shouldAddOverlay ? [self snapshotOverlays] : nil;
  return [_webControllerSnapshotHelper
      generateSnapshotForWebController:self.webController
                          withOverlays:overlays
                      visibleFrameOnly:visibleFrameOnly];
}

- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled {
  [_webControllerSnapshotHelper
      setSnapshotCoalescingEnabled:snapshotCoalescingEnabled];
}

- (CGRect)snapshotContentArea {
  CGRect snapshotContentArea = CGRectZero;
  if (self.tabSnapshottingDelegate) {
    snapshotContentArea =
        [self.tabSnapshottingDelegate snapshotContentAreaForTab:self];
  } else {
    UIEdgeInsets visiblePageInsets = UIEdgeInsetsMake(
        [self headerHeightForWebController:self.webController], 0.0, 0.0, 0.0);
    snapshotContentArea = UIEdgeInsetsInsetRect(self.webController.view.bounds,
                                                visiblePageInsets);
  }
  return snapshotContentArea;
}

- (void)willUpdateSnapshot {
  if ([[self.webController nativeController]
          respondsToSelector:@selector(willUpdateSnapshot)]) {
    [[self.webController nativeController] willUpdateSnapshot];
  }
  [_overscrollActionsController clear];
}

- (void)removeSnapshot {
  DCHECK(self.tabId);
  [self.snapshotManager removeImageWithSessionID:self.tabId];
}

#pragma mark - CRWWebDelegate and CRWWebStateObserver protocol methods

- (void)webStateDidSuppressDialog:(web::WebState*)webState {
  DCHECK(_isPrerenderTab);
  [delegate_ discardPrerender];
}

- (CGFloat)headerHeightForWebController:(CRWWebController*)webController {
  return [self.tabHeadersDelegate headerHeightForTab:self];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    // Disable fullscreen if SSL cert is invalid.
    web::NavigationItem* item = [self navigationManager]->GetTransientItem();
    if (item) {
      web::SecurityStyle securityStyle = item->GetSSL().security_style;
      if (securityStyle == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
        [_legacyFullscreenController disableFullScreen];
      }
    }
  }

  [_parentTabModel notifyTabChanged:self];
  [self updateFullscreenWithToolbarVisible:YES];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  DCHECK(webState == _webStateImpl);
  if (!base::FeatureList::IsEnabled(features::kNewFullscreen)) {
    UIApplicationState state =
        [UIApplication sharedApplication].applicationState;
    if (webState->IsVisible() && state == UIApplicationStateActive) {
      [_legacyFullscreenController disableFullScreen];
    }
  }
  [self.dialogDelegate cancelDialogForTab:self];
}

- (void)webController:(CRWWebController*)webController
    didLoadPassKitObject:(NSData*)data {
  [self.passKitDialogProvider presentPassKitDialog:data];
}

- (void)discardPrerender {
  DCHECK(_isPrerenderTab);
  [delegate_ discardPrerender];
}

- (BOOL)isPrerenderTab {
  return _isPrerenderTab;
}

- (void)wasShown {
  [self updateFullscreenWithToolbarVisible:YES];
  if (self.webState)
    self.webState->WasShown();
}

- (void)wasHidden {
  [self updateFullscreenWithToolbarVisible:YES];
  if (self.webState)
    self.webState->WasHidden();
}

#pragma mark - PagePlaceholderTabHelperDelegate

- (void)displayPlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper {
  // Lazily create page placeholder view.
  if (!_pagePlaceholder) {
    _pagePlaceholder = [[UIImageView alloc] init];
    _pagePlaceholder.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _pagePlaceholder.contentMode = UIViewContentModeScaleAspectFill;
  }

  // Update page placeholder image.
  _pagePlaceholder.image = [CRWWebController defaultSnapshotImage];
  [self getPlaceholderOverlayImageWithCompletionHandler:^(UIImage* image) {
    _pagePlaceholder.image = image;
  }];

  // Display the placeholder on top of WebState's view.
  UIView* webStateView = self.webState->GetView();
  _pagePlaceholder.frame = webStateView.bounds;
  [webStateView addSubview:_pagePlaceholder];
}

// Removes page placeholder view with fade-out animation.
- (void)removePlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper {
  __weak UIView* weakPagePlaceholder = _pagePlaceholder;
  [UIView animateWithDuration:0.5
      animations:^{
        weakPagePlaceholder.alpha = 0.0f;
      }
      completion:^(BOOL finished) {
        [weakPagePlaceholder removeFromSuperview];
      }];
}

@end

#pragma mark - TestingSupport

@implementation Tab (TestingSupport)

- (void)replaceExternalAppLauncher:(id)externalAppLauncher {
  _externalAppLauncher = externalAppLauncher;
}

- (TabModel*)parentTabModel {
  return _parentTabModel;
}

// TODO(crbug.com/620465): this require the Tab's WebState to be a WebStateImpl,
// remove this helper once this is no longer true (and fix the unit tests).
- (web::NavigationManagerImpl*)navigationManagerImpl {
  return static_cast<web::NavigationManagerImpl*>(self.navigationManager);
}

// TODO(crbug.com/620465): this require the Tab's WebState to be a WebStateImpl,
// remove this helper once this is no longer true (and fix the unit tests).
- (CRWWebController*)webController {
  return _webStateImpl ? _webStateImpl->GetWebController() : nil;
}

@end
