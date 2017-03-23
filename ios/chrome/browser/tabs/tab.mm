// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab.h"

#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/ios/block_types.h"
#import "base/ios/weak_nsobject.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/google/core/browser/google_util.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/navigation_metrics/origins_seen_service.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_metrics.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/crash_loop_detection_util.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/metrics/ios_chrome_origins_seen_service_factory.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#import "ios/chrome/browser/passwords/passwords_ui_delegate_impl.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#include "ios/chrome/browser/signin/account_consistency_service_factory.h"
#include "ios/chrome/browser/signin/account_reconcilor_factory.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
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
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/downloads/download_manager_controller.h"
#import "ios/chrome/browser/ui/fullscreen_controller.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/prerender_delegate.h"
#import "ios/chrome/browser/ui/reader_mode/reader_mode_checker.h"
#import "ios/chrome/browser/ui/reader_mode/reader_mode_controller.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/web/auto_reload_bridge.h"
#import "ios/chrome/browser/web/external_app_launcher.h"
#import "ios/chrome/browser/web/navigation_manager_util.h"
#import "ios/chrome/browser/web/passkit_dialog_provider.h"
#include "ios/chrome/browser/web/print_observer.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_whitelist_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/favicon_url.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/navigation_context.h"
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

using base::UserMetricsAction;
using web::NavigationManagerImpl;
using net::RequestTracker;

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
class TabHistoryContext;
class FaviconDriverObserverBridge;
class TabInfoBarObserver;

// The key under which the Tab ID is stored in the WebState's serializable user
// data.
NSString* const kTabIDKey = @"TabID";

// The key under which the opener Tab ID is stored in the WebState's
// serializable user data.
NSString* const kOpenerIDKey = @"OpenerID";

// The key under which the opener navigation index is stored in the WebState's
// serializable user data.
NSString* const kOpenerNavigationIndexKey = @"OpenerNavigationIndex";

// Name of histogram for recording the state of the tab when the renderer is
// terminated.
const char kRendererTerminationStateHistogram[] =
    "Tab.StateAtRendererTermination";

// Referrer used for clicks on article suggestions on the NTP.
const char kChromeContentSuggestionsReferrer[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";

// Enum corresponding to UMA's TabForegroundState, for
// Tab.StateAtRendererTermination. Must be kept in sync with the UMA enum.
enum class RendererTerminationTabState {
  // These two values are for when the app is in the foreground.
  FOREGROUND_TAB_FOREGROUND_APP = 0,
  BACKGROUND_TAB_FOREGROUND_APP,
  // These are for when the app is in the background.
  FOREGROUND_TAB_BACKGROUND_APP,
  BACKGROUND_TAB_BACKGROUND_APP,
  TERMINATION_TAB_STATE_COUNT
};
}  // namespace

@interface Tab ()<CRWWebStateObserver,
                  FindInPageControllerDelegate,
                  ReaderModeControllerDelegate> {
  TabModel* parentTabModel_;               // weak
  ios::ChromeBrowserState* browserState_;  // weak

  base::scoped_nsobject<OpenInController> openInController_;
  base::WeakNSProtocol<id<PassKitDialogProvider>> passKitDialogProvider_;

  // Whether or not this tab is currently being displayed.
  BOOL visible_;

  // Holds entries that need to be added to the history DB.  Prerender tabs do
  // not write navigation data to the history DB.  Instead, they cache history
  // data in this vector and add it to the DB when the prerender status is
  // removed (when the Tab is swapped in as a real Tab).
  std::vector<history::HistoryAddPageArgs> addPageVector_;

  // YES if this Tab is being prerendered.
  BOOL isPrerenderTab_;

  // YES if this Tab was initiated from a voice search.
  BOOL isVoiceSearchResultsTab_;

  // YES if the Tab needs to be reloaded after the app becomes active.
  BOOL requireReloadAfterBecomingActive_;

  // Last visited timestamp.
  double lastVisitedTimestamp_;

  base::mac::ObjCPropertyReleaser propertyReleaser_Tab_;

  id<TabDelegate> delegate_;  // weak
  base::WeakNSProtocol<id<TabDialogDelegate>> dialogDelegate_;
  base::WeakNSProtocol<id<SnapshotOverlayProvider>> snapshotOverlayProvider_;

  // Delegate used for snapshotting geometry.
  id<TabSnapshottingDelegate> tabSnapshottingDelegate_;  // weak

  // The Full Screen Controller responsible for hiding/showing the toolbar.
  base::scoped_nsobject<FullScreenController> fullScreenController_;

  // The delegate responsible for headers over the tab.
  id<TabHeadersDelegate> tabHeadersDelegate_;  // weak

  base::WeakNSProtocol<id<FullScreenControllerDelegate>>
      fullScreenControllerDelegate_;

  // The Overscroll controller responsible for displaying the
  // overscrollActionsView above the toolbar.
  base::scoped_nsobject<OverscrollActionsController>
      overscrollActionsController_;
  base::WeakNSProtocol<id<OverscrollActionsControllerDelegate>>
      overscrollActionsControllerDelegate_;

  base::scoped_nsobject<NSString> tabId_;

  // Lightweight object dealing with various different UI behaviours when
  // opening a URL in an external application.
  base::scoped_nsobject<ExternalAppLauncher> externalAppLauncher_;

  // Handles suggestions for form entry.
  base::scoped_nsobject<FormSuggestionController> suggestionController_;

  // Manages the input accessory view during form input.
  base::scoped_nsobject<FormInputAccessoryViewController>
      inputAccessoryViewController_;

  // TODO(crbug.com/661665): move the WebContentsObservers into their own
  // container.
  // Handles saving and autofill of passwords.
  base::scoped_nsobject<PasswordController> passwordController_;

  // Handles autofill.
  base::scoped_nsobject<AutofillController> autofillController_;

  // Handles GAL infobar on web pages.
  base::scoped_nsobject<NativeAppNavigationController>
      nativeAppNavigationController_;

  // Handles caching and retrieving of snapshots.
  base::scoped_nsobject<SnapshotManager> snapshotManager_;

  // Handles retrieving, generating and updating snapshots of CRWWebController's
  // web page.
  base::scoped_nsobject<WebControllerSnapshotHelper>
      webControllerSnapshotHelper_;

  // Handles support for window.print JavaScript calls.
  std::unique_ptr<PrintObserver> printObserver_;

  // AutoReloadBridge for this tab.
  base::scoped_nsobject<AutoReloadBridge> autoReloadBridge_;

  // WebStateImpl for this tab.
  std::unique_ptr<web::WebStateImpl> webStateImpl_;

  // Allows Tab to conform CRWWebStateDelegate protocol.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserver_;

  // Context used by history to scope the lifetime of navigation entry
  // references to Tab.
  std::unique_ptr<TabHistoryContext> tabHistoryContext_;

  // The controller for everything related to reader mode.
  base::scoped_nsobject<ReaderModeController> readerModeController_;

  // C++ bridge that receives notifications from the FaviconDriver.
  std::unique_ptr<FaviconDriverObserverBridge> faviconDriverObserverBridge_;

  // U2F call controller object.
  base::scoped_nsobject<U2FController> U2FController_;

  // C++ observer used to trigger snapshots after the removal of InfoBars.
  std::unique_ptr<TabInfoBarObserver> tabInfoBarObserver_;
}

// Returns the tab's reader mode controller. May contain nil if the feature is
// disabled.
@property(nonatomic, readonly) ReaderModeController* readerModeController;

// Returns a list of FormSuggestionProviders to be queried for suggestions
// in order of priority.
- (NSArray*)suggestionProviders;

// Returns a list of FormInputAccessoryViewProviders to be queried for an input
// accessory view in order of priority.
- (NSArray*)accessoryViewProviders;

// Sets the favicon on the current NavigationItem.
- (void)setFavicon:(const gfx::Image*)image;

// Saves the current title to the history database.
- (void)saveTitleToHistoryDB;

// Adds the current session entry to this history database.
- (void)addCurrentEntryToHistoryDB;

// Adds any cached entries from |addPageVector_| to the history DB.
- (void)commitCachedEntriesToHistoryDB;

// Returns the OpenInController for this tab.
- (OpenInController*)openInController;

// Initialize the Native App Launcher controller.
- (void)initNativeAppNavigationController;

// Opens a link in an external app. Returns YES iff |url| is launched in an
// external app.
- (BOOL)openExternalURL:(const GURL&)url linkClicked:(BOOL)linkClicked;

// Handles exportable files if possible.
- (void)handleExportableFile:(net::HttpResponseHeaders*)headers;

// Called after the session history is replaced, useful for updating members
// with new sessionID.
- (void)didReplaceSessionHistory;

// Called when the UIApplication's state becomes active.
- (void)applicationDidBecomeActive;

@end

namespace {
// TabHistoryContext is used by history to scope the lifetime of navigation
// entry references to Tab.
class TabHistoryContext : public history::Context {
 public:
  TabHistoryContext() {}
  ~TabHistoryContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TabHistoryContext);
};

class FaviconDriverObserverBridge : public favicon::FaviconDriverObserver {
 public:
  FaviconDriverObserverBridge(Tab* owner,
                              favicon::FaviconDriver* favicon_driver);
  ~FaviconDriverObserverBridge() override;

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  Tab* owner_;  // Owns this instance.
  ScopedObserver<favicon::FaviconDriver, favicon::FaviconDriverObserver>
      scoped_observer_;
  DISALLOW_COPY_AND_ASSIGN(FaviconDriverObserverBridge);
};

FaviconDriverObserverBridge::FaviconDriverObserverBridge(
    Tab* owner,
    favicon::FaviconDriver* favicon_driver)
    : owner_(owner), scoped_observer_(this) {
  scoped_observer_.Add(favicon_driver);
}

FaviconDriverObserverBridge::~FaviconDriverObserverBridge() {}

void FaviconDriverObserverBridge::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  [owner_ setFavicon:&image];
}

// Observer class that listens for infobar signals.
class TabInfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  explicit TabInfoBarObserver(Tab* owner);
  ~TabInfoBarObserver() override;
  void SetShouldObserveInfoBarManager(bool should_observe);
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

 private:
  Tab* owner_;  // Owns this instance;
  ScopedObserver<infobars::InfoBarManager, TabInfoBarObserver> scoped_observer_;
  DISALLOW_COPY_AND_ASSIGN(TabInfoBarObserver);
};

TabInfoBarObserver::TabInfoBarObserver(Tab* owner)
    : owner_(owner), scoped_observer_(this) {}

TabInfoBarObserver::~TabInfoBarObserver() {}

void TabInfoBarObserver::SetShouldObserveInfoBarManager(bool should_observe) {
  infobars::InfoBarManager* infobar_manager = [owner_ infoBarManager];
  if (!infobar_manager)
    return;

  if (should_observe) {
    if (!scoped_observer_.IsObserving(infobar_manager))
      scoped_observer_.Add(infobar_manager);
  } else {
    scoped_observer_.Remove(infobar_manager);
  }
}

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

@synthesize browserState = browserState_;
@synthesize useGreyImageCache = useGreyImageCache_;
@synthesize isPrerenderTab = isPrerenderTab_;
@synthesize isLinkLoadingPrerenderTab = isLinkLoadingPrerenderTab_;
@synthesize isVoiceSearchResultsTab = isVoiceSearchResultsTab_;
@synthesize delegate = delegate_;
@synthesize tabSnapshottingDelegate = tabSnapshottingDelegate_;
@synthesize tabHeadersDelegate = tabHeadersDelegate_;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              opener:(Tab*)opener
                         openedByDOM:(BOOL)openedByDOM
                               model:(TabModel*)parentModel {
  web::WebState::CreateParams params(browserState);
  params.created_with_opener = openedByDOM;
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  if ([opener navigationManager]) {
    web::SerializableUserDataManager* userDataManager =
        web::SerializableUserDataManager::FromWebState(webState.get());
    userDataManager->AddSerializableData(opener.tabId, kOpenerIDKey);
    userDataManager->AddSerializableData(
        @([opener navigationManager]->GetLastCommittedItemIndex()),
        kOpenerNavigationIndexKey);
  }

  return [self initWithWebState:std::move(webState) model:parentModel];
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                           model:(TabModel*)parentModel {
  return [self initWithWebState:std::move(webState)
                          model:parentModel
               attachTabHelpers:YES];
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                           model:(TabModel*)parentModel
                attachTabHelpers:(BOOL)attachTabHelpers {
  DCHECK(webState);
  self = [super init];
  if (self) {
    propertyReleaser_Tab_.Init(self, [Tab class]);
    tabHistoryContext_.reset(new TabHistoryContext());
    parentTabModel_ = parentModel;
    browserState_ =
        ios::ChromeBrowserState::FromBrowserState(webState->GetBrowserState());

    webStateImpl_.reset(static_cast<web::WebStateImpl*>(webState.release()));
    webStateObserver_.reset(
        new web::WebStateObserverBridge(self.webState, self));
    [self updateLastVisitedTimestamp];

    // Do not respect |attachTabHelpers| as this tab helper is required for
    // proper conversion from WebState to Tab.
    LegacyTabHelper::CreateForWebState(self.webState, self);

    [self.webController setDelegate:self];

    snapshotManager_.reset([[SnapshotManager alloc] init]);
    webControllerSnapshotHelper_.reset([[WebControllerSnapshotHelper alloc]
        initWithSnapshotManager:snapshotManager_
                            tab:self]);

    [self initNativeAppNavigationController];

    if (attachTabHelpers) {
      AttachTabHelpers(self.webState);

      tabInfoBarObserver_.reset(new TabInfoBarObserver(self));
      tabInfoBarObserver_->SetShouldObserveInfoBarManager(true);

      if (experimental_flags::IsAutoReloadEnabled()) {
        autoReloadBridge_.reset([[AutoReloadBridge alloc] initWithTab:self]);
      }
      printObserver_.reset(new PrintObserver(self.webState));

      base::scoped_nsprotocol<id<PasswordsUiDelegate>> passwordsUiDelegate(
          [[PasswordsUiDelegateImpl alloc] init]);
      passwordController_.reset([[PasswordController alloc]
             initWithWebState:self.webState
          passwordsUiDelegate:passwordsUiDelegate]);
      password_manager::PasswordGenerationManager* passwordGenerationManager =
          [passwordController_ passwordGenerationManager];
      autofillController_.reset([[AutofillController alloc]
               initWithBrowserState:browserState_
          passwordGenerationManager:passwordGenerationManager
                           webState:self.webState]);
      suggestionController_.reset([[FormSuggestionController alloc]
          initWithWebState:self.webState
                 providers:[self suggestionProviders]]);
      inputAccessoryViewController_.reset(
          [[FormInputAccessoryViewController alloc]
              initWithWebState:self.webState
                     providers:[self accessoryViewProviders]]);

      [self setShouldObserveFaviconChanges:YES];

      if (parentModel && parentModel.syncedWindowDelegate) {
        IOSChromeSessionTabHelper::FromWebState(self.webState)
            ->SetWindowID(parentModel.sessionID);
      }

      // Create the ReaderModeController immediately so it can register for
      // WebState changes.
      if (experimental_flags::IsReaderModeEnabled()) {
        readerModeController_.reset([[ReaderModeController alloc]
            initWithWebState:self.webState
                    delegate:self]);
      }
    }

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (NSArray*)accessoryViewProviders {
  NSMutableArray* providers = [NSMutableArray array];
  id<FormInputAccessoryViewProvider> provider =
      [passwordController_ accessoryViewProvider];
  if (provider)
    [providers addObject:provider];
  [providers addObject:[suggestionController_ accessoryViewProvider]];
  return providers;
}

- (NSArray*)suggestionProviders {
  NSMutableArray* providers = [NSMutableArray array];
  [providers addObject:[passwordController_ suggestionProvider]];
  [providers addObject:[autofillController_ suggestionProvider]];
  return providers;
}

- (id<FindInPageControllerDelegate>)findInPageControllerDelegate {
  return self;
}

+ (Tab*)preloadingTabWithBrowserState:(ios::ChromeBrowserState*)browserState
                                  url:(const GURL&)URL
                             referrer:(const web::Referrer&)referrer
                           transition:(ui::PageTransition)transition
                             provider:(id<CRWNativeContentProvider>)provider
                               opener:(Tab*)opener
                     desktopUserAgent:(BOOL)desktopUserAgent
                        configuration:(void (^)(Tab*))configuration {
  Tab* tab = [[[Tab alloc] initWithBrowserState:browserState
                                         opener:opener
                                    openedByDOM:NO
                                          model:nil] autorelease];
  if (desktopUserAgent)
    [tab enableDesktopUserAgent];
  [[tab webController] setNativeProvider:provider];
  [[tab webController] setWebUsageEnabled:YES];

  if (configuration)
    configuration(tab);

  web::NavigationManager::WebLoadParams params(URL);
  params.transition_type = transition;
  params.referrer = referrer;
  [[tab webController] loadWithParams:params];

  return tab;
}

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
  // Note that -[CRWWebController close] has already been called, so nothing
  // significant should be done with it in this method.
  DCHECK_NE(self.webController.delegate, self);
  [super dealloc];
}

- (void)setParentTabModel:(TabModel*)model {
  DCHECK(!model || !parentTabModel_);
  parentTabModel_ = model;

  if (parentTabModel_.syncedWindowDelegate) {
    IOSChromeSessionTabHelper::FromWebState(self.webState)
        ->SetWindowID(model.sessionID);
  }
}

- (NSString*)description {
  return [NSString stringWithFormat:@"%p ... %@ - %s", self, self.title,
                                    self.url.spec().c_str()];
}

- (CRWWebController*)webController {
  return webStateImpl_ ? webStateImpl_->GetWebController() : nil;
}

- (id<TabDialogDelegate>)dialogDelegate {
  return dialogDelegate_;
}

- (BOOL)loadFinished {
  return [self.webController loadPhase] == web::PAGE_LOADED;
}

- (void)setDialogDelegate:(id<TabDialogDelegate>)dialogDelegate {
  dialogDelegate_.reset(dialogDelegate);
}

- (void)setIsVoiceSearchResultsTab:(BOOL)isVoiceSearchResultsTab {
  // There is intentionally no equality check in this setter, as we want the
  // notificaiton to be sent regardless of whether the value has changed.
  isVoiceSearchResultsTab_ = isVoiceSearchResultsTab;
  [parentTabModel_ notifyTabChanged:self];
}

- (PasswordController*)passwordController {
  return passwordController_.get();
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  [webControllerSnapshotHelper_
      retrieveSnapshotForWebController:self.webController
                             sessionID:self.tabId
                          withOverlays:[self snapshotOverlays]
                              callback:callback];
}

- (const GURL&)url {
  // See note in header; this method should be removed.
  web::NavigationItem* item =
      [[self navigationManagerImpl]->GetSessionController() currentItem];
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

- (NSString*)title {
  base::string16 title = self.webState->GetTitle();
  if (title.empty())
    title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  return base::SysUTF16ToNSString(title);
}

- (NSString*)originalTitle {
  // Do not use self.webState->GetTitle() as it returns the display title,
  // not the original page title.
  DCHECK([self navigationManager]);
  web::NavigationItem* item = [self navigationManager]->GetLastCommittedItem();
  if (!item)
    return nil;
  base::string16 pageTitle = item->GetTitle();
  return pageTitle.empty() ? nil : base::SysUTF16ToNSString(pageTitle);
}

- (NSString*)urlDisplayString {
  base::string16 urlText = url_formatter::FormatUrl(
      self.url, url_formatter::kFormatUrlOmitNothing, net::UnescapeRule::SPACES,
      nullptr, nullptr, nullptr);
  return base::SysUTF16ToNSString(urlText);
}

- (NSString*)tabId {
  if (tabId_)
    return tabId_.get();

  DCHECK(self.webState);
  web::SerializableUserDataManager* userDataManager =
      web::SerializableUserDataManager::FromWebState(self.webState);
  NSString* tabId = base::mac::ObjCCast<NSString>(
      userDataManager->GetValueForSerializationKey(kTabIDKey));

  if (!tabId || ![tabId length]) {
    tabId = [[NSUUID UUID] UUIDString];
    userDataManager->AddSerializableData(tabId, kTabIDKey);
  }

  tabId_.reset([tabId copy]);
  return tabId_.get();
}

- (NSString*)openerID {
  DCHECK(self.webState);
  web::SerializableUserDataManager* userDataManager =
      web::SerializableUserDataManager::FromWebState(self.webState);
  id<NSCoding> openerID =
      userDataManager->GetValueForSerializationKey(kOpenerIDKey);
  return base::mac::ObjCCastStrict<NSString>(openerID);
}

- (NSInteger)openerNavigationIndex {
  DCHECK(self.webState);
  web::SerializableUserDataManager* userDataManager =
      web::SerializableUserDataManager::FromWebState(self.webState);
  id<NSCoding> openerNavigationIndex =
      userDataManager->GetValueForSerializationKey(kOpenerNavigationIndexKey);
  if (!openerNavigationIndex)
    return -1;
  return base::mac::ObjCCastStrict<NSNumber>(openerNavigationIndex)
      .integerValue;
}

- (web::WebState*)webState {
  return webStateImpl_.get();
}

- (void)fetchFavicon {
  const GURL& url = self.url;
  if (!url.is_valid())
    return;

  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(self.webState);
  if (faviconDriver) {
    faviconDriver->FetchFavicon(url);
  }
}

- (void)setFavicon:(const gfx::Image*)image {
  web::NavigationItem* item = [self navigationManager]->GetVisibleItem();
  if (!item)
    return;
  if (image) {
    item->GetFavicon().image = *image;
    item->GetFavicon().valid = true;
  }
  [parentTabModel_ notifyTabChanged:self];
}

- (UIImage*)favicon {
  DCHECK([self navigationManager]);
  web::NavigationItem* item = [self navigationManager]->GetVisibleItem();
  if (!item)
    return nil;
  const gfx::Image& image = item->GetFavicon().image;
  if (image.IsEmpty())
    return nil;
  return image.ToUIImage();
}

- (UIView*)view {
  // Record reload of previously-evicted tab.
  if (![self.webController isViewAlive] && [parentTabModel_ tabUsageRecorder]) {
    [parentTabModel_ tabUsageRecorder]->RecordPageLoadStart(self);
  }
  return self.webState ? self.webState->GetView() : nil;
}

- (UIView*)viewForPrinting {
  return self.webController.viewForPrinting;
}

- (web::NavigationManager*)navigationManager {
  return self.webState ? self.webState->GetNavigationManager() : nullptr;
}

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return self.webState ? &(webStateImpl_->GetNavigationManagerImpl()) : nullptr;
}

// Swap out the existing session history with a new list of navigations. Forces
// the tab to reload to update the UI accordingly. This is ok because none of
// the session history is stored in the tab; it's always fetched through the
// navigation manager.
- (void)replaceHistoryWithNavigations:
            (const std::vector<sessions::SerializedNavigationEntry>&)navigations
                         currentIndex:(NSInteger)currentIndex {
  std::vector<std::unique_ptr<web::NavigationItem>> items =
      sessions::IOSSerializedNavigationBuilder::ToNavigationItems(navigations);
  [self navigationManagerImpl]->ReplaceSessionHistory(std::move(items),
                                                      currentIndex);
  [self didReplaceSessionHistory];

  [self.webController loadCurrentURL];
}

- (void)didReplaceSessionHistory {
  // Replace fullScreenController_ with a new sessionID  when the navigation
  // manager changes.
  // TODO(crbug.com/661666): Consider just updating sessionID and not replacing
  // |fullScreenController_|.
  if (fullScreenController_) {
    [fullScreenController_ invalidate];
    [self.webController removeObserver:fullScreenController_];
    fullScreenController_.reset([[FullScreenController alloc]
         initWithDelegate:fullScreenControllerDelegate_
        navigationManager:self.navigationManager
                sessionID:self.tabId]);
    [self.webController addObserver:fullScreenController_];
    // If the content of the page was loaded without knowledge of the
    // toolbar position it will be misplaced under the toolbar instead of
    // right below. This happens e.g. in the case of preloading. This is to make
    // sure the content is moved to the right place.
    [fullScreenController_ moveContentBelowHeader];
  }
}

- (void)setIsLinkLoadingPrerenderTab:(BOOL)isLinkLoadingPrerenderTab {
  isLinkLoadingPrerenderTab_ = isLinkLoadingPrerenderTab;
  [self setIsPrerenderTab:isLinkLoadingPrerenderTab];
}

- (void)setIsPrerenderTab:(BOOL)isPrerender {
  if (isPrerenderTab_ == isPrerender)
    return;

  isPrerenderTab_ = isPrerender;

  self.webController.shouldSuppressDialogs =
      (isPrerender && !isLinkLoadingPrerenderTab_);

  if (isPrerenderTab_)
    return;

  [fullScreenController_ moveContentBelowHeader];
  [self commitCachedEntriesToHistoryDB];
  [self saveTitleToHistoryDB];

  // If the page has finished loading, take a snapshot.  If the page is still
  // loading, do nothing, as CRWWebController will automatically take a
  // snapshot once the load completes.
  BOOL loadingFinished = self.webController.loadPhase == web::PAGE_LOADED;
  if (loadingFinished)
    [self updateSnapshotWithOverlay:YES visibleFrameOnly:YES];

  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForTab:self
               loadSuccess:loadingFinished];
  [self countMainFrameLoad];
}

- (id<FullScreenControllerDelegate>)fullScreenControllerDelegate {
  return fullScreenControllerDelegate_.get();
}

- (void)setFullScreenControllerDelegate:
    (id<FullScreenControllerDelegate>)fullScreenControllerDelegate {
  if (fullScreenControllerDelegate == fullScreenControllerDelegate_) {
    return;
  }
  // Lazily create a FullScreenController.
  // The check for fullScreenControllerDelegate is necessary to avoid recreating
  // a FullScreenController during teardown.
  if (!fullScreenController_ && fullScreenControllerDelegate) {
    fullScreenController_.reset([[FullScreenController alloc]
         initWithDelegate:fullScreenControllerDelegate
        navigationManager:self.navigationManager
                sessionID:self.tabId]);
    [self.webController addObserver:fullScreenController_];
    // If the content of the page was loaded without knowledge of the
    // toolbar position it will be misplaced under the toolbar instead of
    // right below. This happens e.g. in the case of preloading. This is to make
    // sure the content is moved to the right place.
    [fullScreenController_ moveContentBelowHeader];
  }
  fullScreenControllerDelegate_.reset(fullScreenControllerDelegate);
}

- (OverscrollActionsController*)overscrollActionsController {
  return overscrollActionsController_.get();
}

- (id<OverscrollActionsControllerDelegate>)overscrollActionsControllerDelegate {
  return overscrollActionsControllerDelegate_.get();
}

- (void)setOverscrollActionsControllerDelegate:
    (id<OverscrollActionsControllerDelegate>)
        overscrollActionsControllerDelegate {
  if (overscrollActionsControllerDelegate_ ==
      overscrollActionsControllerDelegate)
    return;

  // Lazily create a OverscrollActionsController.
  // The check for overscrollActionsControllerDelegate is necessary to avoid
  // recreating a OverscrollActionsController during teardown.
  if (!overscrollActionsController_) {
    overscrollActionsController_.reset(
        [[OverscrollActionsController alloc] init]);
    [self.webController addObserver:overscrollActionsController_];
  }
  OverscrollStyle style = OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  if (browserState_->IsOffTheRecord()) {
    style = OverscrollStyle::REGULAR_PAGE_INCOGNITO;
  }
  [overscrollActionsController_ setStyle:style];
  [overscrollActionsController_
      setDelegate:overscrollActionsControllerDelegate];
  overscrollActionsControllerDelegate_.reset(
      overscrollActionsControllerDelegate);
}

- (void)saveTitleToHistoryDB {
  // If incognito, don't update history.
  if (browserState_->IsOffTheRecord())
    return;
  // Don't update the history if current entry has no title.
  NSString* title = [self title];
  if (![title length] ||
      [title isEqualToString:l10n_util::GetNSString(IDS_DEFAULT_TAB_TITLE)])
    return;

  history::HistoryService* historyService =
      ios::HistoryServiceFactory::GetForBrowserState(
          browserState_, ServiceAccessType::IMPLICIT_ACCESS);
  DCHECK(historyService);
  historyService->SetPageTitle(self.url, base::SysNSStringToUTF16(title));
}

- (void)addCurrentEntryToHistoryDB {
  DCHECK([self navigationManager]->GetVisibleItem());
  // If incognito, don't update history.
  if (browserState_->IsOffTheRecord())
    return;

  web::NavigationItem* item = [self navigationManager]->GetVisibleItem();

  // Do not update the history db for back/forward navigations.
  // TODO(crbug.com/661667): We do not currently tag the entry with a
  // FORWARD_BACK transition. Fix.
  if (item->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK)
    return;

  history::HistoryService* historyService =
      ios::HistoryServiceFactory::GetForBrowserState(
          browserState_, ServiceAccessType::IMPLICIT_ACCESS);
  DCHECK(historyService);

  const GURL url(item->GetURL());
  const web::Referrer& referrer = item->GetReferrer();

// Do not update the history db for data: urls.  This diverges from upstream,
// but prevents us from dumping huge view-source urls into the history
// database.  Since view-source is only activated in Debug builds, this check
// can be Debug-only as well.
#ifndef NDEBUG
  if (url.scheme() == url::kDataScheme)
    return;
#endif

  history::RedirectList redirects;
  GURL originalURL = item->GetOriginalRequestURL();
  if (item->GetURL() != originalURL) {
    // Simulate a valid redirect chain in case of URL that have been modified
    // in |CRWWebController finishHistoryNavigationFromEntry:|.
    const std::string& urlSpec = item->GetURL().spec();
    size_t urlSpecLength = urlSpec.size();
    if (item->GetTransitionType() & ui::PAGE_TRANSITION_CLIENT_REDIRECT ||
        (urlSpecLength && (urlSpec.at(urlSpecLength - 1) == '#') &&
         !urlSpec.compare(0, urlSpecLength - 1, originalURL.spec()))) {
      redirects.push_back(referrer.url);
    }
    // TODO(crbug.com/661670): the redirect chain is not constructed the same
    // way as upstream so this part needs to be revised.
    redirects.push_back(originalURL);
    redirects.push_back(url);
  }

  DCHECK(item->GetTimestamp().ToInternalValue() > 0);
  if ([self isPrerenderTab]) {
    // Clicks on content suggestions on the NTP should not contribute to the
    // Most Visited tiles in the NTP.
    const bool consider_for_ntp_most_visited =
        referrer.url != GURL(kChromeContentSuggestionsReferrer);

    history::HistoryAddPageArgs args(
        url, item->GetTimestamp(), tabHistoryContext_.get(),
        item->GetUniqueID(), referrer.url, redirects, item->GetTransitionType(),
        history::SOURCE_BROWSED, false, consider_for_ntp_most_visited);
    addPageVector_.push_back(args);
  } else {
    historyService->AddPage(url, item->GetTimestamp(), tabHistoryContext_.get(),
                            item->GetUniqueID(), referrer.url, redirects,
                            item->GetTransitionType(), history::SOURCE_BROWSED,
                            false);
    [self saveTitleToHistoryDB];
  }
}

- (void)commitCachedEntriesToHistoryDB {
  // If OTR, don't update history.
  if (browserState_->IsOffTheRecord()) {
    DCHECK_EQ(0U, addPageVector_.size());
    return;
  }

  history::HistoryService* historyService =
      ios::HistoryServiceFactory::GetForBrowserState(
          browserState_, ServiceAccessType::IMPLICIT_ACCESS);
  DCHECK(historyService);

  for (size_t i = 0; i < addPageVector_.size(); ++i)
    historyService->AddPage(addPageVector_[i]);
  addPageVector_.clear();
}

- (void)webWillInitiateLoadWithParams:
    (web::NavigationManager::WebLoadParams&)params {
  GURL navUrl = params.url;

  // After a crash the NTP is loaded by default.
  if (navUrl.host() != kChromeUINewTabHost) {
    static BOOL hasLoadedPage = NO;
    if (!hasLoadedPage) {
      // As soon as an URL is loaded, a crash shouldn't be counted as a startup
      // crash. Since loading an url requires user action and is a significant
      // source of crashes that could lead to false positives in crash loop
      // detection.
      crash_util::ResetFailedStartupAttemptCount();
      hasLoadedPage = YES;
    }
  }
}

- (void)webDidUpdateSessionForLoadWithParams:
            (const web::NavigationManager::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation {
  GURL navUrl = params.url;
  ui::PageTransition transition = params.transition_type;

  // Record any explicit, non-redirect navigation as a clobber (as long as it's
  // in a real tab).
  if (!initialNavigation && !isPrerenderTab_ &&
      !PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) &&
      (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) == 0) {
    base::RecordAction(UserMetricsAction("MobileTabClobbered"));
  }
  if ([parentTabModel_ tabUsageRecorder])
    [parentTabModel_ tabUsageRecorder]->RecordPageLoadStart(self);

  // Reset |isVoiceSearchResultsTab| since a new page is being navigated to.
  self.isVoiceSearchResultsTab = NO;

  web::NavigationItem* navigationItem =
      [self navigationManager]->GetPendingItem();

  // TODO(crbug.com/676129): the pending item is not correctly set when the
  // page is reloading, use the last committed item if pending item is null.
  // Remove this once tracking bug is fixed.
  if (!navigationItem)
    navigationItem = [self navigationManager]->GetLastCommittedItem();

  [[OmniboxGeolocationController sharedInstance]
      addLocationToNavigationItem:navigationItem
                     browserState:browserState_];
}

- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab {
  DCHECK(sessionTab);
  [self replaceHistoryWithNavigations:sessionTab->navigations
                         currentIndex:sessionTab->current_navigation_index];
}

- (void)webWillReload {
  if ([parentTabModel_ tabUsageRecorder]) {
    [parentTabModel_ tabUsageRecorder]->RecordReload(self);
  }
}

// Stop the page loading.
// Equivalent to the user pressing 'stop', or a window.stop() command.
- (void)stopLoading {
  [self.webController stopLoading];
}

// Halt the tab, which amounts to halting its webController.
- (void)terminateNetworkActivity {
  [self.webController terminateNetworkActivity];
}

// This can't be done in dealloc in case someone holds an extra strong
// reference to the Tab, which would cause the close sequence to fire at a
// random time.
- (void)close {
  self.fullScreenControllerDelegate = nil;
  self.overscrollActionsControllerDelegate = nil;
  self.passKitDialogProvider = nil;
  self.snapshotOverlayProvider = nil;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [passwordController_ detach];
  passwordController_.reset();
  tabInfoBarObserver_.reset();

  faviconDriverObserverBridge_.reset();
  [openInController_ detachFromWebController];
  openInController_.reset();
  [autofillController_ detachFromWebState];
  [suggestionController_ detachFromWebState];
  if (fullScreenController_)
    [self.webController removeObserver:fullScreenController_];
  [fullScreenController_ invalidate];
  fullScreenController_.reset();
  if (overscrollActionsController_)
    [self.webController removeObserver:overscrollActionsController_];
  [overscrollActionsController_ invalidate];
  overscrollActionsController_.reset();
  [readerModeController_ detachFromWebState];
  readerModeController_.reset();

  // Invalidate any snapshot stored for this session.
  DCHECK(self.tabId);
  [snapshotManager_ removeImageWithSessionID:self.tabId];
  // Reset association with the webController.
  [self.webController setDelegate:nil];

  // Cancel any queued dialogs.
  [self.dialogDelegate cancelDialogForTab:self];

  // These steps must be done last, and must be done in this order; nothing
  // involving the tab should be done after didCloseTab:, and the
  // CRWWebController backing the tab should outlive anything done during tab
  // closure (since -[CRWWebController close] is what begins tearing down the
  // web/ layer, and tab closure may trigger operations that need to query the
  // web/ layer). The scoped strong ref is because didCloseTab: is often the
  // trigger for deallocating the tab, but that can in turn cause
  // CRWWebController to be deallocated before its close is called.  The facade
  // delegate should be torn down after |-didCloseTab:| so components triggered
  // by tab closure can use the content facade, and it should be deleted before
  // the web controller since the web controller owns the facade's backing
  // objects. |parentTabModel_| is reset after calling |-didCloseTab:| to
  // prevent propagating WebState notifications that happen during WebState
  // destruction.
  // TODO(crbug.com/546222): Fix the need for this; TabModel should be
  // responsible for making the lifetime of Tab sane, rather than allowing Tab
  // to drive its own destruction.
  base::scoped_nsobject<Tab> kungFuDeathGrip([self retain]);
  [parentTabModel_ didCloseTab:self];  // Inform parent of tab closure.
  parentTabModel_ = nil;

  // Destroy the WebState but first stop listening to WebState events (as |self|
  // is in no state to respond to the notifications if |webStateImpl_| is null).
  LegacyTabHelper::RemoveFromWebState(self.webState);
  webStateObserver_.reset();
  webStateImpl_.reset();
}

- (void)dismissModals {
  [openInController_ disable];
  [self.webController dismissModals];
}

- (void)setShouldObserveInfoBarManager:(BOOL)shouldObserveInfoBarManager {
  tabInfoBarObserver_->SetShouldObserveInfoBarManager(
      shouldObserveInfoBarManager);
}

- (void)setShouldObserveFaviconChanges:(BOOL)shouldObserveFaviconChanges {
  if (shouldObserveFaviconChanges) {
    favicon::FaviconDriver* faviconDriver =
        favicon::WebFaviconDriver::FromWebState(self.webState);
    // Some MockWebContents used in tests do not support the FaviconDriver.
    if (faviconDriver) {
      faviconDriverObserverBridge_.reset(
          new FaviconDriverObserverBridge(self, faviconDriver));
    }
  } else {
    faviconDriverObserverBridge_.reset();
  }
}

- (void)goBack {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoBack());
    base::RecordAction(UserMetricsAction("Back"));
    self.navigationManager->GoBack();
  }
}

- (void)goForward {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoForward());
    base::RecordAction(UserMetricsAction("Forward"));
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

  if (self.navigationManager) {
    CRWSessionController* sessionController =
        [self navigationManagerImpl]->GetSessionController();
    NSInteger itemIndex = [sessionController indexOfItem:item];
    DCHECK_NE(itemIndex, NSNotFound);
    self.navigationManager->GoToIndex(itemIndex);
  }
}

- (BOOL)openExternalURL:(const GURL&)url linkClicked:(BOOL)linkClicked {
  if (!externalAppLauncher_.get())
    externalAppLauncher_.reset([[ExternalAppLauncher alloc] init]);

  // This method may release CRWWebController which may cause a crash
  // (crbug.com/393949).
  [[self.webController retain] autorelease];

  // Make a local url copy for possible modification.
  GURL finalURL = url;

  // Check if it's a direct FIDO U2F x-callback call. If so, do not open it, to
  // prevent pages from spoofing requests with different origins.
  if (finalURL.SchemeIs("u2f-x-callback")) {
    return NO;
  }

  // Check if it's a FIDO U2F call.
  if (finalURL.SchemeIs("u2f")) {
    // Create U2FController object lazily.
    if (!U2FController_) {
      U2FController_.reset([[U2FController alloc] init]);
    }

    DCHECK([self navigationManager]);
    GURL origin =
        [self navigationManager]->GetLastCommittedItem()->GetURL().GetOrigin();

    // Compose u2f-x-callback URL and update urlToOpen.
    finalURL = [U2FController_ XCallbackFromRequestURL:finalURL
                                             originURL:origin
                                                tabURL:self.url
                                                 tabID:self.tabId];

    if (!finalURL.is_valid()) {
      return NO;
    }
  }

  if ([externalAppLauncher_ openURL:finalURL linkClicked:linkClicked]) {
    // Clears pending navigation history after successfully launching the
    // external app.
    DCHECK([self navigationManager]);
    [self navigationManager]->DiscardNonCommittedItems();
    // Ensure the UI reflects the current entry, not the just-discarded pending
    // entry.
    [parentTabModel_ notifyTabChanged:self];
    return YES;
  }
  return NO;
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (navigation->IsSameDocument()) {
    auto* faviconDriver = favicon::WebFaviconDriver::FromWebState(webState);
    if (faviconDriver) {
      // Fetch the favicon for the new URL.
      faviconDriver->FetchFavicon(navigation->GetUrl());
    }
  }

  if (!navigation->IsErrorPage()) {
    [self addCurrentEntryToHistoryDB];
    [self countMainFrameLoad];
  }

  [parentTabModel_ notifyTabChanged:self];
}

// Records the state (scroll position, form values, whatever can be
// harvested) from the current page into the current session entry.
- (void)recordStateInHistory {
  // Link-loading prerender tab may not have correct zoom value during the load.
  if (!self.isLinkLoadingPrerenderTab)
    [self.webController recordStateInHistory];
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
  if (!openInController_) {
    openInController_.reset([[OpenInController alloc]
        initWithRequestContext:browserState_->GetRequestContext()
                 webController:self.webController]);
  }
  return openInController_.get();
}

- (id<CRWNativeContent>)controllerForUnhandledContentAtURL:(const GURL&)url {
  // Shows download manager UI for unhandled content.
  DownloadManagerController* downloadController =
      [[[DownloadManagerController alloc] initWithWebState:self.webState
                                               downloadURL:url] autorelease];
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
  // at the last component of |self.url| and if both of these fail use the
  // default filename "document".
  std::string contentDisposition;
  if (headers)
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  std::string defaultFilename =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  base::string16 filename =
      net::GetSuggestedFilename(self.url, contentDisposition,
                                "",                 // referrer-charset
                                "",                 // suggested-name
                                "application/pdf",  // mime-type
                                defaultFilename);
  [[self openInController]
      enableWithDocumentURL:self.url
          suggestedFilename:base::SysUTF16ToNSString(filename)];
}

- (void)countMainFrameLoad {
  if ([self isPrerenderTab] || [self url].SchemeIs(kChromeUIScheme)) {
    return;
  }
  base::RecordAction(UserMetricsAction("MobilePageLoaded"));
}

- (void)applicationDidBecomeActive {
  if (requireReloadAfterBecomingActive_) {
    if (visible_) {
      self.navigationManager->Reload(web::ReloadType::NORMAL,
                                     false /* check_for_repost */);
    } else {
      [self.webController requirePageReload];
    }
    requireReloadAfterBecomingActive_ = NO;
  }
}

#pragma mark -
#pragma mark FindInPageControllerDelegate

- (void)willAdjustScrollPosition {
  // Skip the next attempt to correct the scroll offset for the toolbar height.
  // Used when programatically scrolling down the y offset.
  [fullScreenController_ shouldSkipNextScrollOffsetForHeader];
}

#pragma mark -
#pragma mark FullScreen

- (void)updateFullscreenWithToolbarVisible:(BOOL)visible {
  [fullScreenController_ moveHeaderToRestingPosition:visible];
}

#pragma mark -
#pragma mark Reader mode

- (UIView*)superviewForReaderModePanel {
  return self.view;
}

- (ReaderModeController*)readerModeController {
  return readerModeController_.get();
}

- (BOOL)canSwitchToReaderMode {
  // Only if the page is loaded and the page passes suitability checks.
  ReaderModeController* controller = self.readerModeController;
  return controller && controller.checker->CanSwitchToReaderMode();
}

- (void)switchToReaderMode {
  DCHECK(self.view);
  [self.readerModeController switchToReaderMode];
}

- (void)loadReaderModeHTML:(NSString*)html forURL:(const GURL&)url {
  // Before changing the HTML on the current page, this checks that the URL has
  // not changed since reader mode was requested. This could happen for example
  // if the page does a late redirect itself or if the user tapped on a link and
  // triggered reader mode before the page load is detected by webState.
  if (url == self.url)
    [self.webController loadHTMLForCurrentURL:html];

  [self.readerModeController exitReaderMode];
}

#pragma mark -

- (BOOL)usesDesktopUserAgent {
  if (!self.navigationManager)
    return NO;

  web::NavigationItem* visibleItem = self.navigationManager->GetVisibleItem();
  return visibleItem &&
         visibleItem->GetUserAgentType() == web::UserAgentType::DESKTOP;
}

- (void)enableDesktopUserAgent {
  DCHECK_EQ(self.usesDesktopUserAgent, NO);
  DCHECK([self navigationManager]);
  [self navigationManager]->OverrideDesktopUserAgentForNextPendingItem();
}

- (void)reloadForDesktopUserAgent {
  // This removes the web view, which will be recreated at the end of this.
  [self.webController requirePageReconstruction];

  // TODO(crbug.com/228171): A hack in session_controller -addPendingItem
  // discusses making tab responsible for distinguishing history stack
  // navigation from new navigations.
  web::NavigationManager* navigationManager = [self navigationManager];
  DCHECK(navigationManager);
  web::NavigationItem* lastNonRedirectedItem =
      GetLastNonRedirectedItem(navigationManager);
  if (!lastNonRedirectedItem)
    return;

  // |reloadURL| will be empty if a page was open by DOM.
  GURL reloadURL(lastNonRedirectedItem->GetOriginalRequestURL());
  if (reloadURL.is_empty()) {
    DCHECK(self.webState && self.webState->HasOpener());
    reloadURL = lastNonRedirectedItem->GetVirtualURL();
  }

  web::NavigationManager::WebLoadParams params(reloadURL);
  params.referrer = lastNonRedirectedItem->GetReferrer();
  // A new navigation is needed here for reloading with desktop User-Agent.
  params.transition_type =
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT);
  navigationManager->LoadURLWithParams(params);
}

- (id<SnapshotOverlayProvider>)snapshotOverlayProvider {
  return snapshotOverlayProvider_.get();
}

- (void)setSnapshotOverlayProvider:
    (id<SnapshotOverlayProvider>)snapshotOverlayProvider {
  snapshotOverlayProvider_.reset(snapshotOverlayProvider);
}

- (void)evaluateU2FResultFromURL:(const GURL&)URL {
  DCHECK(U2FController_);
  [U2FController_ evaluateU2FResultFromU2FURL:URL webState:self.webState];
}

#pragma mark - CRWWebDelegate and CRWWebStateObserver protocol methods.

// This method is invoked whenever the system believes the URL is about to
// change, or immediately after any unexpected change of the URL. The apparent
// destination URL is included in the |url| parameter.
// Warning: because of the present design it is possible for malicious websites
// to invoke superflous instances of this delegate with artibrary URLs.
// Ensure there is nothing here that could be a risk to the user beyond mild
// confusion in that event (e.g. progress bar starting unexpectedly).
- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition {
  DCHECK(self.webController.loadPhase == web::LOAD_REQUESTED);
  DCHECK([self navigationManager]);

  // Move the toolbar to visible during page load.
  [fullScreenController_ disableFullScreen];

  BOOL isUserNavigationEvent =
      (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) == 0;
  // Check for link-follow clobbers. These are changes where there is no
  // pending entry (since that means the change wasn't caused by this class),
  // and where the URL changes (to avoid counting page resurrection).
  // TODO(crbug.com/546401): Consider moving this into NavigationManager, or
  // into a NavigationManager observer callback, so it doesn't need to be
  // checked in several places.
  if (isUserNavigationEvent && !isPrerenderTab_ &&
      ![self navigationManager]->GetPendingItem() && url != self.url) {
    base::RecordAction(UserMetricsAction("MobileTabClobbered"));
    if ([parentTabModel_ tabUsageRecorder])
      [parentTabModel_ tabUsageRecorder]->RecordPageLoadStart(self);
  }
  if (![self navigationManager]->GetPendingItem()) {
    // Reset |isVoiceSearchResultsTab| since a new page is being navigated to.
    self.isVoiceSearchResultsTab = NO;
  }
}

- (void)webState:(web::WebState*)webState
    didStartProvisionalNavigationForURL:(const GURL&)URL {
  [self.dialogDelegate cancelDialogForTab:self];
  [parentTabModel_ notifyTabChanged:self];
  [openInController_ disable];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:
          kTabClosingCurrentDocumentNotificationForCrashReporting
                    object:self];
}

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  DCHECK([self navigationManager]);
  // |webWillAddPendingURL:transition:| is not called for native page loads.
  // TODO(crbug.com/381201): Move this call there once that bug is fixed so that
  // |disableFullScreen| is called only from one place.
  [fullScreenController_ disableFullScreen];
  GURL lastCommittedURL = webState->GetLastCommittedURL();
  [autoReloadBridge_ loadStartedForURL:lastCommittedURL];

  if (parentTabModel_) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabWillStartLoadingNotification
                      object:parentTabModel_
                    userInfo:@{kTabModelTabKey : self}];
  }
  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (faviconDriver) {
    faviconDriver->FetchFavicon(lastCommittedURL);
  }
  [parentTabModel_ notifyTabChanged:self];
  if (parentTabModel_) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabDidStartLoadingNotification
                      object:parentTabModel_
                    userInfo:@{kTabModelTabKey : self}];
  }

  web::NavigationItem* previousItem = nullptr;
  if (details.previous_item_index >= 0) {
    previousItem = webState->GetNavigationManager()->GetItemAtIndex(
        details.previous_item_index);
  }

  [parentTabModel_ navigationCommittedInTab:self previousItem:previousItem];

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
  DCHECK(self.webController.loadPhase == web::PAGE_LOADED);

  // Cancel prerendering if response is "application/octet-stream". It can be a
  // video file which should not be played from preload tab (crbug.com/436813).
  if (isPrerenderTab_ &&
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
  if (loadSuccess)
    [autoReloadBridge_ loadFinishedForURL:lastCommittedURL wasPost:wasPost];
  else
    [autoReloadBridge_ loadFailedForURL:lastCommittedURL wasPost:wasPost];
  [webControllerSnapshotHelper_ setSnapshotCoalescingEnabled:YES];
  if (!loadSuccess) {
    [fullScreenController_ disableFullScreen];
  }
  [self recordInterfaceOrientation];
  navigation_metrics::OriginsSeenService* originsSeenService =
      IOSChromeOriginsSeenServiceFactory::GetForBrowserState(self.browserState);
  bool alreadySeen = originsSeenService->Insert(url::Origin(lastCommittedURL));
  navigation_metrics::RecordMainFrameNavigation(
      lastCommittedURL, true, self.browserState->IsOffTheRecord(), alreadySeen);

  if (loadSuccess) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        webStateImpl_->GetHttpResponseHeaders();
    [self handleExportableFile:headers.get()];
  }

  [parentTabModel_ notifyTabChanged:self];

  if (parentTabModel_) {
    if ([parentTabModel_ tabUsageRecorder])
      [parentTabModel_ tabUsageRecorder]->RecordPageLoadDone(self, loadSuccess);
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabModelTabDidFinishLoadingNotification
                      object:parentTabModel_
                    userInfo:[NSDictionary
                                 dictionaryWithObjectsAndKeys:
                                     self, kTabModelTabKey,
                                     [NSNumber numberWithBool:loadSuccess],
                                     kTabModelPageLoadSuccess, nil]];
  }
  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForTab:self
               loadSuccess:loadSuccess];

  if (loadSuccess) {
    [self updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
  }
  [webControllerSnapshotHelper_ setSnapshotCoalescingEnabled:NO];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  // TODO(crbug.com/546406): It is probably possible to do something smarter,
  // but the fact that this is not always sent will have to be taken into
  // account.
  [parentTabModel_ notifyTabChanged:self];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self saveTitleToHistoryDB];
  [parentTabModel_ notifyTabChanged:self];
}

- (void)webStateDidDismissInterstitial:(web::WebState*)webState {
  [parentTabModel_ notifyTabChanged:self];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  // This is the maximum that a page will ever load and it is safe to allow
  // fullscreen mode.
  [fullScreenController_ enableFullScreen];
  [parentTabModel_ notifyTabChanged:self];
}

- (BOOL)webController:(CRWWebController*)webController
    shouldOpenExternalURL:(const GURL&)URL {
  if (isPrerenderTab_ && !isLinkLoadingPrerenderTab_) {
    [delegate_ discardPrerender];
    return NO;
  }
  return YES;
}

- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)url
                         sourceURL:(const GURL&)sourceURL
                       linkClicked:(BOOL)linkClicked {
  // Don't open any native app directly when prerendering or from Incognito.
  if (isPrerenderTab_ || self.browserState->IsOffTheRecord())
    return NO;

  base::scoped_nsprotocol<id<NativeAppMetadata>> metadata(
      [[ios::GetChromeBrowserProvider()->GetNativeAppWhitelistManager()
          nativeAppForURL:url] retain]);
  if (![metadata shouldAutoOpenLinks])
    return NO;

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState);
  ChromeIdentity* identity = authenticationService->GetAuthenticatedIdentity();

  // Attempts to open external app without x-callback.
  if ([self openExternalURL:[metadata launchURLWithURL:url identity:identity]
                linkClicked:linkClicked]) {
    return YES;
  }

  // Auto-open didn't work. Reset the auto-open flag.
  [metadata unsetShouldAutoOpenLinks];
  return NO;
}

- (double)lastVisitedTimestamp {
  return lastVisitedTimestamp_;
}

- (void)updateLastVisitedTimestamp {
  lastVisitedTimestamp_ = [[NSDate date] timeIntervalSince1970];
}

- (infobars::InfoBarManager*)infoBarManager {
  DCHECK(self.webState);
  return InfoBarManagerImpl::FromWebState(self.webState);
}

- (NSArray*)snapshotOverlays {
  return [snapshotOverlayProvider_ snapshotOverlaysForTab:self];
}

- (void)webViewRemoved {
  [openInController_ disable];
}

- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  // chrome:// URLs are only allowed if the mainDocumentURL is also a chrome://
  // URL.
  if (url.SchemeIs(kChromeUIScheme) &&
      !mainDocumentURL.SchemeIs(kChromeUIScheme)) {
    return NO;
  }

  // Always allow frame loads.
  BOOL isFrameLoad = (url != mainDocumentURL);
  if (isFrameLoad) {
    return YES;
  }

  // TODO(crbug.com/546402): If this turns out to be useful, find a less hacky
  // hook point to send this from.
  NSString* urlString = base::SysUTF8ToNSString(url.spec());
  if ([urlString length]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabUrlMayStartLoadingNotificationForCrashReporting
                      object:self
                    userInfo:[NSDictionary dictionaryWithObject:urlString
                                                         forKey:kTabUrlKey]];
  }

  return YES;
}

- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block {
  NSString* sessionID = self.tabId;
  // The snapshot is always grey, even if |useGreyImageCache_| is NO, as this
  // overlay represents an out-of-date website and is shown only until the
  // has begun loading. However, if |useGreyImageCache_| is YES, the grey image
  // is already cached in memory for swiping, and a cache miss is acceptable.
  // In other cases, such as during startup, either disk access or a greyspace
  // conversion is required, as there will be no grey snapshots in memory.
  if (useGreyImageCache_) {
    [snapshotManager_ greyImageForSessionID:sessionID callback:block];
  } else {
    [webControllerSnapshotHelper_
        retrieveGreySnapshotForWebController:webController
                                   sessionID:sessionID
                                withOverlays:[self snapshotOverlays]
                                    callback:block];
  }
}

- (UIImage*)updateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                     visibleFrameOnly:(BOOL)visibleFrameOnly {
  NSArray* overlays = shouldAddOverlay ? [self snapshotOverlays] : nil;
  UIImage* snapshot = [webControllerSnapshotHelper_
      updateSnapshotForWebController:self.webController
                           sessionID:self.tabId
                        withOverlays:overlays
                    visibleFrameOnly:visibleFrameOnly];
  [parentTabModel_ notifyTabSnapshotChanged:self withImage:snapshot];
  return snapshot;
}

- (UIImage*)generateSnapshotWithOverlay:(BOOL)shouldAddOverlay
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  NSArray* overlays = shouldAddOverlay ? [self snapshotOverlays] : nil;
  return [webControllerSnapshotHelper_
      generateSnapshotForWebController:self.webController
                          withOverlays:overlays
                      visibleFrameOnly:visibleFrameOnly];
}

- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled {
  [webControllerSnapshotHelper_
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
  [overscrollActionsController_ clear];
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  [self.webController setWebUsageEnabled:webUsageEnabled];
}

- (void)webStateDidSuppressDialog:(web::WebState*)webState {
  DCHECK(isPrerenderTab_);
  [delegate_ discardPrerender];
}

- (CGFloat)headerHeightForWebController:(CRWWebController*)webController {
  return [self.tabHeadersDelegate headerHeightForTab:self];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  // Disable fullscreen if SSL cert is invalid.
  web::NavigationItem* item = [self navigationManager]->GetTransientItem();
  if (item) {
    web::SecurityStyle securityStyle = item->GetSSL().security_style;
    if (securityStyle == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
      [fullScreenController_ disableFullScreen];
    }
  }

  [parentTabModel_ notifyTabChanged:self];
  [self updateFullscreenWithToolbarVisible:YES];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  if (browserState_ && !browserState_->IsOffTheRecord()) {
    // Report the crash.
    GetApplicationContext()
        ->GetMetricsServicesManager()
        ->OnRendererProcessCrash();

    // Log the tab state for the termination.
    RendererTerminationTabState tab_state =
        visible_ ? RendererTerminationTabState::FOREGROUND_TAB_FOREGROUND_APP
                 : RendererTerminationTabState::BACKGROUND_TAB_FOREGROUND_APP;
    if ([UIApplication sharedApplication].applicationState ==
        UIApplicationStateBackground) {
      tab_state =
          visible_ ? RendererTerminationTabState::FOREGROUND_TAB_BACKGROUND_APP
                   : RendererTerminationTabState::BACKGROUND_TAB_BACKGROUND_APP;
    }
    UMA_HISTOGRAM_ENUMERATION(
        kRendererTerminationStateHistogram, static_cast<int>(tab_state),
        static_cast<int>(
            RendererTerminationTabState::TERMINATION_TAB_STATE_COUNT));
    if ([parentTabModel_ tabUsageRecorder])
      [parentTabModel_ tabUsageRecorder]->RendererTerminated(self, visible_);
  }

  BOOL applicationIsBackgrounded =
      [UIApplication sharedApplication].applicationState ==
      UIApplicationStateBackground;
  if (visible_) {
    if (!applicationIsBackgrounded) {
      base::WeakNSObject<Tab> weakSelf(self);
      base::scoped_nsobject<SadTabView> sadTabView(
          [[SadTabView alloc] initWithReloadHandler:^{
            base::scoped_nsobject<Tab> strongSelf([weakSelf retain]);

            // |check_for_repost| is true because this is called from SadTab and
            // explicitly initiated by the user.
            [strongSelf navigationManager]->Reload(web::ReloadType::NORMAL,
                                                   true /* check_for_repost */);
          }]);
      base::scoped_nsobject<CRWContentView> contentView(
          [[CRWGenericContentView alloc] initWithView:sadTabView]);
      self.webState->ShowTransientContentView(contentView);
      [fullScreenController_ disableFullScreen];
    }
  } else {
    [self.webController requirePageReload];
  }
  // Returning to the app (after the renderer crashed in the background) and
  // having the page reload is much less confusing for the user.
  // Note: Given that the tab is visible, calling |requirePageReload| will not
  // work when the app becomes active because there is nothing to trigger
  // a view redisplay in that scenario.
  requireReloadAfterBecomingActive_ = visible_ && applicationIsBackgrounded;
  [self.dialogDelegate cancelDialogForTab:self];
}

- (void)webController:(CRWWebController*)webController
    didLoadPassKitObject:(NSData*)data {
  [self.passKitDialogProvider presentPassKitDialog:data];
}

#pragma mark - PrerenderDelegate

- (void)discardPrerender {
  DCHECK(isPrerenderTab_);
  [delegate_ discardPrerender];
}

- (BOOL)isPrerenderTab {
  return isPrerenderTab_;
}

#pragma mark - ManageAccountsDelegate

- (void)onManageAccounts {
  if (isPrerenderTab_) {
    [delegate_ discardPrerender];
    return;
  }
  if (self != [parentTabModel_ currentTab])
    return;

  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(browserState_)
          ->GetState());
  base::scoped_nsobject<GenericChromeCommand> command(
      [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_ACCOUNTS_SETTINGS]);
  [self.view chromeExecuteCommand:command];
}

- (void)onAddAccount {
  if (isPrerenderTab_) {
    [delegate_ discardPrerender];
    return;
  }
  if (self != [parentTabModel_ currentTab])
    return;

  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(browserState_)
          ->GetState());
  base::scoped_nsobject<GenericChromeCommand> command(
      [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_ADD_ACCOUNT]);
  [self.view chromeExecuteCommand:command];
}

- (void)onGoIncognito:(const GURL&)url {
  if (isPrerenderTab_) {
    [delegate_ discardPrerender];
    return;
  }
  if (self != [parentTabModel_ currentTab])
    return;

  // The user taps on go incognito from the mobile U-turn webpage (the web page
  // that displays all users accounts available in the content area). As the
  // user chooses to go to incognito, the mobile U-turn page is no longer
  // neeeded. The current solution is to go back in history. This has the
  // advantage of keeping the current browsing session and give a good user
  // experience when the user comes back from incognito.
  [self goBack];

  if (url.is_valid()) {
    base::scoped_nsobject<OpenUrlCommand> command([[OpenUrlCommand alloc]
         initWithURL:url
            referrer:web::Referrer()  // Strip referrer when switching modes.
         inIncognito:YES
        inBackground:NO
            appendTo:kLastTab]);
    [self.view chromeExecuteCommand:command];
  } else {
    base::scoped_nsobject<GenericChromeCommand> chromeCommand(
        [[GenericChromeCommand alloc] initWithTag:IDC_NEW_INCOGNITO_TAB]);
    [self.view chromeExecuteCommand:chromeCommand];
  }
}

- (NativeAppNavigationController*)nativeAppNavigationController {
  return nativeAppNavigationController_;
}

- (void)initNativeAppNavigationController {
  if (browserState_->IsOffTheRecord())
    return;
  DCHECK(!nativeAppNavigationController_);
  nativeAppNavigationController_.reset(
      [[NativeAppNavigationController alloc] initWithWebState:self.webState]);
  DCHECK(nativeAppNavigationController_);
}

- (id<PassKitDialogProvider>)passKitDialogProvider {
  return passKitDialogProvider_.get();
}

- (void)setPassKitDialogProvider:(id<PassKitDialogProvider>)provider {
  passKitDialogProvider_.reset(provider);
}

- (void)wasShown {
  visible_ = YES;
  [self updateFullscreenWithToolbarVisible:YES];
  [self.webController wasShown];
  [inputAccessoryViewController_ wasShown];
}

- (void)wasHidden {
  visible_ = NO;
  [self updateFullscreenWithToolbarVisible:YES];
  [self.webController wasHidden];
  [inputAccessoryViewController_ wasHidden];
}

@end

#pragma mark - TestingSupport

@implementation Tab (TestingSupport)

- (void)replaceExternalAppLauncher:(id)externalAppLauncher {
  externalAppLauncher_.reset([externalAppLauncher retain]);
}

- (TabModel*)parentTabModel {
  return parentTabModel_;
}

- (FormInputAccessoryViewController*)inputAccessoryViewController {
  return inputAccessoryViewController_.get();
}

@end
