// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#include <memory>
#include <string>

#import <CoreSpotlight/CoreSpotlight.h>
#import <objc/objc.h>
#import <objc/runtime.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/ios/block_types.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_component_installer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/payments/core/features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/url_formatter/url_formatter.h"
#include "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/background_activity.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/app/startup/chrome_main_starter.h"
#include "ios/chrome/app/startup/client_registration.h"
#import "ios/chrome/app/startup/content_suggestions_scheduler_notifications.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/app/startup/network_stack_setup.h"
#include "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/app/startup/register_experimental_settings.h"
#include "ios/chrome/app/startup/setup_debugging.h"
#import "ios/chrome/app/startup_tasks.h"
#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/app_startup_parameters.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"
#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/callback_counter.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_background_uploader.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/file_metadata_util.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher_factory.h"
#import "ios/chrome/browser/payments/payment_request_constants.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/pref_observer_bridge.h"
#import "ios/chrome/browser/reading_list/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/share_extension/share_extension_service.h"
#import "ios/chrome/browser/share_extension/share_extension_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/chrome_web_view_factory.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/downloads/download_manager_controller.h"
#import "ios/chrome/browser/ui/external_file_remover_factory.h"
#import "ios/chrome/browser/ui/external_file_remover_impl.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen_controller.h"
#import "ios/chrome/browser/ui/history/history_panel_view_controller.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/main_coordinator.h"
#import "ios/chrome/browser/ui/main/view_controller_swapping.h"
#import "ios/chrome/browser/ui/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/promos/signin_promo_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#import "ios/chrome/browser/ui/stack_view/stack_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#include "ios/chrome/common/app_group/app_group_utils.h"
#include "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/crn_http_protocol_handler.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MDCTypographyAdditions/MDFRobotoFontLoader+MDCTypographyAdditions.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ios/web/net/request_tracker_factory_impl.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/net/web_http_protocol_handler_delegate.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_capabilities.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_view_creation_util.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "mojo/edk/embedder/embedder.h"
#import "net/base/mac/url_conversions.h"
#include "net/url_request/url_request_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Preference key used to store which profile is current.
NSString* kIncognitoCurrentKey = @"IncognitoActive";

// Constants for deferred initialization of preferences observer.
NSString* const kPrefObserverInit = @"PrefObserverInit";

// Constants for deferring notifying the AuthenticationService of a new cold
// start.
NSString* const kAuthenticationServiceNotification =
    @"AuthenticationServiceNotification";

// Constants for deferring reseting the startup attempt count (to give the app
// a little while to make sure it says alive).
NSString* const kStartupAttemptReset = @"StartupAttempReset";

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

// Constants for deferred check if it is necessary to send pings to
// Chrome distribution related services.
NSString* const kSendInstallPingIfNecessary = @"SendInstallPingIfNecessary";

// Constants for deferring check of native iOS apps installed.
NSString* const kCheckForFirstPartyApps = @"CheckForFirstPartyApps";

// Constants for deferred deletion of leftover user downloaded files.
NSString* const kDeleteDownloads = @"DeleteDownloads";

// Constants for deferred sending of queued feedback.
NSString* const kSendQueuedFeedback = @"SendQueuedFeedback";

// Constants for deferring the deletion of pre-upgrade crash reports.
NSString* const kCleanupCrashReports = @"CleanupCrashReports";

// Constants for deferring the deletion of old snapshots.
NSString* const kPurgeSnapshots = @"PurgeSnapshots";

// Constants for deferring startup Spotlight bookmark indexing.
NSString* const kStartSpotlightBookmarksIndexing =
    @"StartSpotlightBookmarksIndexing";

// Constants for deferred promo display.
const NSTimeInterval kDisplayPromoDelay = 0.1;

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Adapted from chrome/browser/ui/browser_init.cc.
void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      GetApplicationContext()->GetComponentUpdateService();
  DCHECK(cus);
  base::FilePath path;
  const bool success = PathService::Get(ios::DIR_USER_DATA, &path);
  DCHECK(success);
  // CRLSetFetcher attempts to load a CRL set from either the local disk or
  // network.
  component_updater::DeleteLegacyCRLSet(path);
  component_updater::RegisterCRLSetComponent(cus, path);
}

// Used to update the current BVC mode if a new tab is added while the stack
// view is being dimissed.  This is different than ApplicationMode in that it
// can be set to |NONE| when not in use.
enum class StackViewDismissalMode { NONE, NORMAL, INCOGNITO };

// The delay, in seconds, for cleaning external files.
const int kExternalFilesCleanupDelaySeconds = 60;

}  // namespace

@interface MainController ()<BrowserStateStorageSwitching,
                             PrefObserverDelegate,
                             SettingsNavigationControllerDelegate,
                             TabModelObserver,
                             TabSwitcherDelegate,
                             UserFeedbackDataSource> {
  IBOutlet UIWindow* _window;

  // Weak; owned by the ChromeBrowserProvider.
  ios::ChromeBrowserStateManager* _browserStateManager;

  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;

  // The ChromeBrowserState associated with the main (non-OTR) browsing mode.
  ios::ChromeBrowserState* _mainBrowserState;  // Weak.

  // Wrangler to handle BVC and tab model creation, access, and related logic.
  // Implements faetures exposed from this object through the
  // BrowserViewInformation protocol.
  BrowserViewWrangler* _browserViewWrangler;

  // Parameters received at startup time when the app is launched from another
  // app.
  AppStartupParameters* _startupParameters;

  // Navigation View controller for the settings.
  SettingsNavigationController* _settingsNavigationController;

  // View controller for switching tabs.
  UIViewController<TabSwitcher>* _tabSwitcherController;

  // YES while animating the dismissal of stack view.
  BOOL _dismissingStackView;

  // If not NONE, the current BVC should be switched to this BVC on completion
  // of stack view dismissal.
  StackViewDismissalMode _modeToDisplayOnStackViewDismissal;

  // If YES, the tab switcher is currently active.
  BOOL _tabSwitcherIsActive;

  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // Keeps track of the restore state during startup.
  CrashRestoreHelper* _restoreHelper;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

  // RequestTrackerFactory to customize the behavior of the network stack.
  std::unique_ptr<web::RequestTrackerFactoryImpl> _requestTrackerFactory;

  // Configuration for the HTTP protocol handler.
  std::unique_ptr<web::WebHTTPProtocolHandlerDelegate>
      _httpProtocolHandlerDelegate;

  // True if First Run UI (terms of service & sync sign-in) is being presented
  // in a modal dialog.
  BOOL _isPresentingFirstRunUI;

  // The tab switcher command and the voice search commands can be sent by views
  // that reside in a different UIWindow leading to the fact that the exclusive
  // touch property will be ineffective and a command for processing both
  // commands may be sent in the same run of the runloop leading to
  // inconsistencies. Those two boolean indicate if one of those commands have
  // been processed in the last 200ms in order to only allow processing one at
  // a time.
  // TODO(crbug.com/560296):  Provide a general solution for handling mutually
  // exclusive chrome commands sent at nearly the same time.
  BOOL _isProcessingTabSwitcherCommand;
  BOOL _isProcessingVoiceSearchCommand;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _localStatePrefObserverBridge;

  // Registrar for pref changes notifications to the local state.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // Clears browsing data from ChromeBrowserStates.
  BrowsingDataRemovalController* _browsingDataRemovalController;

  // The class in charge of showing/hiding the memory debugger when the
  // appropriate pref changes.
  MemoryDebuggerManager* _memoryDebuggerManager;

  // Responsible for indexing chrome links (such as bookmarks, most likely...)
  // in system Spotlight index.
  SpotlightManager* _spotlightManager;

  // Cached launchOptions from -didFinishLaunchingWithOptions.
  NSDictionary* _launchOptions;

  // View controller for displaying the history panel.
  UIViewController* _historyPanelViewController;

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  // Hander for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;
}

// Pointer to the main view controller, always owned by the main window.
@property(weak, nonatomic, readonly)
    UIViewController<ViewControllerSwapping>* mainViewController;

// The main coordinator, lazily created the first time it is accessed. Manages
// the main view controller. This property should not be accessed before the
// browser has started up to the FOREGROUND stage.
@property(nonatomic, readonly) MainCoordinator* mainCoordinator;

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
@property(nonatomic, readwrite)
    NTPTabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// The SigninInteractionCoordinator to present Sign In UI. It is created the
// first time Sign In UI is needed to be presented and should not be destroyed
// while the UI is presented.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

// Activates browsing and enables web views if |enabled| is YES.
// Disables browsing and purges web views if |enabled| is NO.
// Must be called only on the main thread.
- (void)setWebUsageEnabled:(BOOL)enabled;
// Activates |mainBVC| and |otrBVC| and sets |currentBVC| as primary iff
// |currentBVC| can be made active.
- (void)activateBVCAndMakeCurrentBVCPrimary;
// Sets |currentBVC| as the root view controller for the window.
- (void)displayCurrentBVC;
// Invokes the sign in flow with the specified authentication operation and
// invokes |callback| when finished.
- (void)showSigninWithOperation:(AuthenticationOperation)operation
                       identity:(ChromeIdentity*)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
                    promoAction:(signin_metrics::PromoAction)promoAction
                       callback:(ShowSigninCommandCompletionCallback)callback;
// Shows the tab switcher UI.
- (void)showTabSwitcher;
// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVCWithOriginView:(UIView*)originView;
// Dismisses the tab switcher UI without animation into the given model.
- (void)dismissTabSwitcherWithoutAnimationInModel:(TabModel*)tabModel;
// Dismisses |signinInteractionCoordinator|.
- (void)dismissSigninInteractionCoordinator;
// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed;
// Called when the last regular tab was closed.
- (void)lastRegularTabClosed;
// Post a notification with name |notificationName| on the first available
// run loop cycle.
- (void)postNotificationOnNextRunLoopCycle:(NSString*)notificationName;
// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI, based on the |dismissModals| flag:
// - If a modal dialog is showing and |dismissModals| is NO, the selected tab of
// the main tab model will change in the background, but the view won't change.
// - Otherwise, any modal view will be dismissed, the stack view will animate
// out if it is showing, the target BVC will become active, and the new tab will
// be shown.
// If the current tab in |targetMode| is a NTP, it can be reused to open URL.
// |completion| is executed after the tab is opened.
- (Tab*)openSelectedTabInMode:(ApplicationMode)targetMode
                      withURL:(const GURL&)url
                   transition:(ui::PageTransition)transition
                   completion:(ProceduralBlock)completion;
// Checks the target BVC's current tab's URL. If this URL is chrome://newtab,
// loads |url| in this tab. Otherwise, open |url| in a new tab in the target
// BVC.
// |tabDisplayedCompletion| will be called on the new tab (if not nil).
- (Tab*)openOrReuseTabInMode:(ApplicationMode)targetMode
                     withURL:(const GURL&)url
                  transition:(ui::PageTransition)transition
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion;
// Returns whether the restore infobar should be displayed.
- (bool)mustShowRestoreInfobar;
// Begins the process of dismissing the stack view with the given current model,
// switching which BVC is suspended if necessary, but not updating the UI.
- (void)beginDismissingStackViewWithCurrentModel:(TabModel*)tabModel;
// Completes the process of dismissing the stack view, removing it from the
// screen and showing the appropriate BVC.
- (void)finishDismissingStackView;
// Sets up self.currentBVC for testing by closing existing tabs.
- (void)setUpCurrentBVCForTesting;
// Opens an url from a link in the settings UI.
- (void)openUrlFromSettings:(OpenUrlCommand*)command;
// Switch all global states for the given mode (normal or incognito).
- (void)switchGlobalStateToMode:(ApplicationMode)mode;
// Updates the local storage, cookie store, and sets the global state.
- (void)changeStorageFromBrowserState:(ios::ChromeBrowserState*)oldState
                       toBrowserState:(ios::ChromeBrowserState*)newState;
// Returns the set of the sessions ids of the tabs in the given |tabModel|.
- (NSMutableSet*)liveSessionsForTabModel:(TabModel*)tabModel;
// Purge the unused snapshots.
- (void)purgeSnapshots;
// Sets a LocalState pref marking the TOS EULA as accepted.
- (void)markEulaAsAccepted;
// Sends any feedback that happens to still be on local storage.
- (void)sendQueuedFeedback;
// Called whenever an orientation change is received.
- (void)orientationDidChange:(NSNotification*)notification;
// Register to receive orientation change notification to update breakpad
// report.
- (void)registerForOrientationChangeNotifications;
// Asynchronously creates the pref observers.
- (void)schedulePrefObserverInitialization;
// Asynchronously schedules a check for what other native iOS apps are currently
// installed. Note that there may be App Store restrictions around checking for
// native iOS apps not owned by the browser vendor.
- (void)scheduleCheckForFirstPartyApps;
// Asynchronously schedules pings to distribution services.
- (void)scheduleAppDistributionPings;
// Asynchronously schedule the init of the memoryDebuggerManager.
- (void)scheduleMemoryDebuggingTools;
// Asynchronously kick off regular free memory checks.
- (void)startFreeMemoryMonitoring;
// Asynchronously schedules the notification of the AuthenticationService.
- (void)scheduleAuthenticationServiceNotification;
// Asynchronously schedules the reset of the failed startup attempt counter.
- (void)scheduleStartupAttemptReset;
// Asynchronously schedules the cleanup of crash reports.
- (void)scheduleCrashReportCleanup;
// Asynchronously schedules the deletion of old snapshots.
- (void)scheduleSnapshotPurge;
// Schedules various cleanup tasks that are performed after launch.
- (void)scheduleStartupCleanupTasks;
// Schedules various tasks to be performed after the application becomes active.
- (void)scheduleLowPriorityStartupTasks;
// Schedules tasks that require a fully-functional BVC to be performed.
- (void)scheduleTasksRequiringBVCWithBrowserState;
// Schedules the deletion of user downloaded files that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteDownloadsDirectory;
// Returns whether or not the app can launch in incognito mode.
- (BOOL)canLaunchInIncognito;
// Determines which UI should be shown on startup, and shows it.
- (void)createInitialUI:(ApplicationMode)launchMode;
// Initializes the first run UI and presents it to the user.
- (void)showFirstRunUI;
// Schedules presentation of the first eligible promo found, if any.
- (void)scheduleShowPromo;
// Crashes the application if requested.
- (void)crashIfRequested;
// Returns the BrowsingDataRemovalController. Lazily creates one if necessary.
- (BrowsingDataRemovalController*)browsingDataRemovalController;
// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData;
// Deletes the incognito browser state.
- (void)deleteIncognitoBrowserState;
// Handles the notification that first run modal dialog UI is about to complete.
- (void)handleFirstRunUIWillFinish;
// Handles the notification that first run modal dialog UI completed.
- (void)handleFirstRunUIDidFinish;
// Performs synchronous browser state initialization steps.
- (void)initializeBrowserState:(ios::ChromeBrowserState*)browserState;
// Helper methods to initialize the application to a specific stage.
// Setting |_browserInitializationStage| to a specific stage requires the
// corresponding function to return YES.
// Initializes the application to INITIALIZATION_STAGE_BASIC, which is the
// minimum initialization needed in all cases.
- (void)startUpBrowserBasicInitialization;
// Initializes the application to INITIALIZATION_STAGE_BACKGROUND, which is
// needed by background handlers.
- (void)startUpBrowserBackgroundInitialization;
// Initializes the application to INITIALIZATION_STAGE_FOREGROUND, which is
// needed when application runs in foreground.
- (void)startUpBrowserForegroundInitialization;
@end

@implementation MainController

@synthesize mainCoordinator = _mainCoordinator;
@synthesize appState = _appState;
@synthesize appLaunchTime = _appLaunchTime;
@synthesize browserInitializationStage = _browserInitializationStage;
@synthesize window = _window;
@synthesize isPresentingFirstRunUI = _isPresentingFirstRunUI;
@synthesize isColdStart = _isColdStart;
@synthesize NTPActionAfterTabSwitcherDismissal =
    _NTPActionAfterTabSwitcherDismissal;
@synthesize launchOptions = _launchOptions;
@synthesize startupParameters = _startupParameters;
@synthesize metricsMediator = _metricsMediator;
@synthesize settingsNavigationController = _settingsNavigationController;
@synthesize signinInteractionCoordinator = _signinInteractionCoordinator;

#pragma mark - Application lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _startupTasks = [[StartupTasks alloc] init];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);
  net::RequestTracker::SetRequestTrackerFactory(nullptr);
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

// This function starts up to only what is needed at each stage of the
// initialization. It is possible to continue initialization later.
- (void)startUpBrowserToStage:(BrowserInitializationStageType)stage {
  if (_browserInitializationStage < INITIALIZATION_STAGE_BASIC &&
      stage >= INITIALIZATION_STAGE_BASIC) {
    [self startUpBrowserBasicInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_BASIC;
  }

  if (_browserInitializationStage < INITIALIZATION_STAGE_BACKGROUND &&
      stage >= INITIALIZATION_STAGE_BACKGROUND) {
    [self startUpBrowserBackgroundInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_BACKGROUND;
  }

  if (_browserInitializationStage < INITIALIZATION_STAGE_FOREGROUND &&
      stage >= INITIALIZATION_STAGE_FOREGROUND) {
    // When adding a new initialization flow, consider setting
    // |_appState.userInteracted| at the appropriate time.
    DCHECK(_appState.userInteracted);
    [self startUpBrowserForegroundInitialization];
    _browserInitializationStage = INITIALIZATION_STAGE_FOREGROUND;
  }
}

- (void)startUpBrowserBasicInitialization {
  _appLaunchTime = base::TimeTicks::Now();
  _isColdStart = YES;

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(![self.appState isInSafeMode]);

  NSBundle* baseBundle = base::mac::OuterBundle();
  base::mac::SetBaseBundleID(
      base::SysNSStringToUTF8([baseBundle bundleIdentifier]).c_str());

  // Register default values for experimental settings (Application Preferences)
  // and set the "Version" key in the UserDefaults.
  [RegisterExperimentalSettings
      registerExperimentalSettingsWithUserDefaults:[NSUserDefaults
                                                       standardUserDefaults]
                                            bundle:base::mac::
                                                       FrameworkBundle()];

  // Register all clients before calling any web code.
  [ClientRegistration registerClients];

  _chromeMain = [ChromeMainStarter startChromeMain];

  // Initialize the ChromeBrowserProvider.
  ios::GetChromeBrowserProvider()->Initialize();

  // If the user is interacting, crashes affect the user experience. Start
  // reporting as soon as possible.
  // TODO(crbug.com/507633): Call this even sooner.
  if (_appState.userInteracted)
    GetApplicationContext()->GetMetricsService()->OnAppEnterForeground();

  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());

  // TODO(crbug.com/546171): Audit all the following code to see if some of it
  // should move into BrowserMainParts or BrowserProcess.

  [NetworkStackSetup setUpChromeNetworkStack:&_requestTrackerFactory
                 httpProtocolHandlerDelegate:&_httpProtocolHandlerDelegate];
}

- (void)startUpBrowserForegroundInitialization {
  // Give tests a chance to prepare for testing.
  tests_hook::SetUpTestsIfPresent();

  GetApplicationContext()->OnAppEnterForeground();

  // TODO(crbug.com/546171): Audit all the following code to see if some of it
  // should move into BrowserMainParts or BrowserProcess.
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];

  // Although this duplicates some metrics_service startup logic also in
  // IOSChromeMain(), this call does additional work, checking for wifi-only
  // and setting up the required support structures.
  [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Resets the number of crash reports that have been uploaded since the
  // previous Foreground initialization.
  [CrashReportBackgroundUploader resetReportsUploadedInBackgroundCount];

  // Resets the interval stats between two background fetch as this value may be
  // obsolete.
  [BackgroundActivity foregroundStarted];

  // Crash the app during startup if requested but only after we have enabled
  // uploading crash reports.
  [self crashIfRequested];

  if (experimental_flags::MustClearApplicationGroupSandbox()) {
    // Clear the Application group sandbox if requested. This operation take
    // some time and will access the file system synchronously as the rest of
    // the startup sequence requires it to be completed before continuing.
    app_group::ClearAppGroupSandbox();
  }

  RegisterComponentsForUpdate();

  // Remove the extra browser states as Chrome iOS is single profile in M48+.
  ChromeBrowserStateRemovalController::GetInstance()
      ->RemoveBrowserStatesIfNecessary();

  _browserStateManager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  ios::ChromeBrowserState* chromeBrowserState =
      _browserStateManager->GetLastUsedBrowserState();

  // The CrashRestoreHelper must clean up the old browser state information
  // before the tabModels can be created.  |_restoreHelper| must be kept alive
  // until the BVC receives the browser state and tab model.
  BOOL postCrashLaunch = [self mustShowRestoreInfobar];
  if (postCrashLaunch) {
    _restoreHelper =
        [[CrashRestoreHelper alloc] initWithBrowserState:chromeBrowserState];
    [_restoreHelper moveAsideSessionInformation];
  }

  // Initialize and set the main browser state.
  [self initializeBrowserState:chromeBrowserState];
  _mainBrowserState = chromeBrowserState;
  [_browserViewWrangler shutdown];
  _browserViewWrangler =
      [[BrowserViewWrangler alloc] initWithBrowserState:_mainBrowserState
                                       tabModelObserver:self
                             applicationCommandEndpoint:self];

  // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
  // start.
  feature_engagement::TrackerFactory::GetForBrowserState(chromeBrowserState)
      ->NotifyEvent(feature_engagement::events::kChromeOpened);

  // Ensure the main tab model is created.
  ignore_result([_browserViewWrangler mainTabModel]);

  _spotlightManager =
      [SpotlightManager spotlightManagerWithBrowserState:_mainBrowserState];

  ShareExtensionService* service =
      ShareExtensionServiceFactory::GetForBrowserState(_mainBrowserState);
  service->Initialize();

  // Before bringing up the UI, make sure the launch mode is correct, and
  // check for previous crashes.
  BOOL startInIncognito = [standardDefaults boolForKey:kIncognitoCurrentKey];
  BOOL switchFromIncognito = startInIncognito && ![self canLaunchInIncognito];

  if (postCrashLaunch || switchFromIncognito) {
    [self clearIOSSpecificIncognitoData];
    if (switchFromIncognito)
      [self switchGlobalStateToMode:ApplicationMode::NORMAL];
  }
  if (switchFromIncognito)
    startInIncognito = NO;

  [MDCTypography setFontLoader:[MDFRobotoFontLoader sharedInstance]];

  [self createInitialUI:(startInIncognito ? ApplicationMode::INCOGNITO
                                          : ApplicationMode::NORMAL)];

  [self scheduleStartupCleanupTasks];
  [MetricsMediator logLaunchMetricsWithStartupInformation:self
                                   browserViewInformation:_browserViewWrangler];
  if (self.isColdStart) {
    [ContentSuggestionsSchedulerNotifications
        notifyColdStart:_mainBrowserState];
    [ContentSuggestionsSchedulerNotifications
        notifyForeground:_mainBrowserState];
  }

  [self scheduleLowPriorityStartupTasks];

  [_browserViewWrangler updateDeviceSharingManager];

  [self openTabFromLaunchOptions:_launchOptions
              startupInformation:self
                        appState:self.appState];
  _launchOptions = nil;

  if (!_startupParameters) {
    // The startup parameters may create new tabs or navigations. If the restore
    // infobar is displayed now, it may be dismissed immediately and the user
    // will never be able to restore the session.
    [_restoreHelper showRestoreIfNeeded:[self currentTabModel]];
    _restoreHelper = nil;
  }

  [self scheduleTasksRequiringBVCWithBrowserState];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();
}

- (void)initializeBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(!browserState->IsOffTheRecord());
  search_engines::UpdateSearchEnginesIfNeeded(
      browserState->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

- (void)handleFirstRunUIWillFinish {
  DCHECK(_isPresentingFirstRunUI);
  _isPresentingFirstRunUI = NO;
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];

  [self markEulaAsAccepted];

  if (_startupParameters) {
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                                        withURL:[_startupParameters externalURL]
                                 dismissOmnibox:YES
                                     transition:ui::PAGE_TRANSITION_LINK
                                     completion:^{
                                       [self setStartupParameters:nil];
                                     }];
  }
}

- (void)handleFirstRunUIDidFinish {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIDidFinishNotification
              object:nil];

  // As soon as First Run has finished, give OmniboxGeolocationController an
  // opportunity to present the iOS system location alert.
  [[OmniboxGeolocationController sharedInstance]
      triggerSystemPromptForNewUser:YES];
}

- (void)clearIOSSpecificIncognitoData {
  DCHECK(_mainBrowserState->HasOffTheRecordChromeBrowserState());
  ios::ChromeBrowserState* otrBrowserState =
      _mainBrowserState->GetOffTheRecordChromeBrowserState();
  int removeAllMask = ~0;
  void (^completion)() = ^{
    [self activateBVCAndMakeCurrentBVCPrimary];
  };
  [self.browsingDataRemovalController
      removeIOSSpecificIncognitoBrowsingDataFromBrowserState:otrBrowserState
                                                        mask:removeAllMask
                                           completionHandler:completion];
}

- (void)deleteIncognitoBrowserState {
  BOOL otrBVCIsCurrent = (self.currentBVC == self.otrBVC);

  // If the current BVC is the otr BVC, then the user should be in the card
  // stack, this is not true for the iPad tab switcher.
  DCHECK(IsIPadIdiom() || (!otrBVCIsCurrent || _tabSwitcherIsActive));

  // We always clear the otr tab model on iPad.
  // Notify the _tabSwitcherController that its otrBVC will be destroyed.
  if (IsIPadIdiom() || _tabSwitcherIsActive)
    [_tabSwitcherController setOtrTabModel:nil];

  [_browserViewWrangler
      deleteIncognitoTabModelState:self.browsingDataRemovalController];

  if (otrBVCIsCurrent) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr tab model on iPad with tab switcher enabled.
  // Notify the _tabSwitcherController with the new otrBVC.
  if (IsIPadIdiom() || _tabSwitcherIsActive)
    [_tabSwitcherController setOtrTabModel:self.otrTabModel];
}

- (BrowsingDataRemovalController*)browsingDataRemovalController {
  if (!_browsingDataRemovalController) {
    _browsingDataRemovalController =
        [[BrowsingDataRemovalController alloc] init];
  }
  return _browsingDataRemovalController;
}

- (void)setWebUsageEnabled:(BOOL)enabled {
  DCHECK([NSThread isMainThread]);
  if (enabled) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  } else {
    [self.mainBVC setActive:NO];
    [self.otrBVC setActive:NO];
  }
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback for |removeBrowsingDataFromBrowserState:| is received.
  if (![self.browsingDataRemovalController
          hasPendingRemovalOperations:self.currentBrowserState]) {
    [self.mainBVC setActive:YES];
    [self.otrBVC setActive:YES];

    [self.currentBVC setPrimary:YES];
  }
}

#pragma mark - Property implementation.

- (id<BrowserViewInformation>)browserViewInformation {
  return _browserViewWrangler;
}

- (UIViewController<ViewControllerSwapping>*)mainViewController {
  return self.mainCoordinator.mainViewController;
}

- (MainCoordinator*)mainCoordinator {
  if (_browserInitializationStage == INITIALIZATION_STAGE_BASIC) {
    NOTREACHED() << "mainCoordinator accessed too early in initialization.";
    return nil;
  }
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    _mainCoordinator = [[MainCoordinator alloc] initWithWindow:self.window];
  }
  return _mainCoordinator;
}

- (BOOL)isFirstLaunchAfterUpgrade {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

#pragma mark - StartupInformation implementation.

- (FirstUserActionRecorder*)firstUserActionRecorder {
  return _firstUserActionRecorder.get();
}

- (void)resetFirstUserActionRecorder {
  _firstUserActionRecorder.reset();
}

- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay {
  [self performSelector:@selector(expireFirstUserActionRecorder)
             withObject:nil
             afterDelay:delay];
}

- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime {
  base::TimeDelta delta = base::TimeDelta::FromSeconds(backgroundTime);
  _firstUserActionRecorder.reset(new FirstUserActionRecorder(delta));
}

- (void)stopChromeMain {
  GetApplicationContext()->SetIsShuttingDown();

  [_spotlightManager shutdown];
  _spotlightManager = nil;

  [_browserViewWrangler shutdown];
  _browserViewWrangler = nil;

  _chromeMain.reset();
}

- (BOOL)isTabSwitcherActive {
  return _tabSwitcherIsActive;
}

#pragma mark - BrowserViewInformation implementation.

- (void)haltAllTabs {
  [_browserViewWrangler haltAllTabs];
}

- (void)cleanDeviceSharingManager {
  [_browserViewWrangler cleanDeviceSharingManager];
}

#pragma mark - Startup tasks

- (void)sendQueuedFeedback {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendQueuedFeedback
                  block:^{
                    ios::GetChromeBrowserProvider()
                        ->GetUserFeedbackProvider()
                        ->Synchronize();
                  }];
}

- (void)orientationDidChange:(NSNotification*)notification {
  breakpad_helper::SetCurrentOrientation(
      [[UIApplication sharedApplication] statusBarOrientation],
      [[UIDevice currentDevice] orientation]);
}

- (void)registerForOrientationChangeNotifications {
  // Register to both device orientation and UI orientation did change
  // notification as these two events may be triggered independantely.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIApplicationDidChangeStatusBarOrientationNotification
           object:nil];
}

- (void)schedulePrefObserverInitialization {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPrefObserverInit
                  block:^{
                    // Track changes to local state prefs.
                    _localStatePrefObserverBridge.reset(
                        new PrefObserverBridge(self));
                    _localStatePrefChangeRegistrar.Init(
                        GetApplicationContext()->GetLocalState());
                    _localStatePrefObserverBridge->ObserveChangesForPreference(
                        metrics::prefs::kMetricsReportingEnabled,
                        &_localStatePrefChangeRegistrar);
                    _localStatePrefObserverBridge->ObserveChangesForPreference(
                        prefs::kMetricsReportingWifiOnly,
                        &_localStatePrefChangeRegistrar);

                    // Calls the onPreferenceChanged function in case there was
                    // a
                    // change to the observed preferences before the observer
                    // bridge was set up.
                    [self onPreferenceChanged:metrics::prefs::
                                                  kMetricsReportingEnabled];
                    [self onPreferenceChanged:prefs::kMetricsReportingWifiOnly];
                  }];
}

- (void)scheduleCheckForFirstPartyApps {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCheckForFirstPartyApps
                  block:^{
                    ios::GetChromeBrowserProvider()->CheckForFirstPartyApps();
                  }];
}

- (void)scheduleAppDistributionPings {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendInstallPingIfNecessary
                  block:^{
                    net::URLRequestContextGetter* context =
                        _mainBrowserState->GetRequestContext();
                    bool is_first_run = FirstRun::IsChromeFirstRun();
                    ios::GetChromeBrowserProvider()
                        ->GetAppDistributionProvider()
                        ->ScheduleDistributionNotifications(context,
                                                            is_first_run);
                  }];
}

- (void)scheduleAuthenticationServiceNotification {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kAuthenticationServiceNotification
                  block:^{
                    DCHECK(_mainBrowserState);
                    // Force an obvious initialization of the
                    // AuthenticationService.
                    // This is done for clarity purpose only, and should be
                    // removed
                    // alongside the delayed initialization. See
                    // crbug.com/464306.
                    AuthenticationServiceFactory::GetForBrowserState(
                        _mainBrowserState);
                    if (![self currentBrowserState]) {
                      // Active browser state should have been set before
                      // scheduling
                      // any authentication service notification.
                      NOTREACHED();
                      return;
                    }
                    if ([SignedInAccountsViewController
                            shouldBePresentedForBrowserState:
                                [self currentBrowserState]]) {
                      [self
                          presentSignedInAccountsViewControllerForBrowserState:
                              [self currentBrowserState]];
                    }
                  }];
}

- (void)scheduleStartupAttemptReset {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartupAttemptReset
                  block:^{
                    crash_util::ResetFailedStartupAttemptCount();
                  }];
}

- (void)scheduleCrashReportCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupCrashReports
                  block:^{
                    breakpad_helper::CleanupCrashReports();
                  }];
}

- (void)scheduleSnapshotPurge {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPurgeSnapshots
                  block:^{
                    [self purgeSnapshots];
                  }];
}

- (void)scheduleStartupCleanupTasks {
  // Cleanup crash reports if this is the first run after an update.
  if ([self isFirstLaunchAfterUpgrade]) {
    [self scheduleCrashReportCleanup];
  }

  // ClearSessionCookies() is not synchronous.
  if (cookie_util::ShouldClearSessionCookies()) {
    cookie_util::ClearSessionCookies(
        _mainBrowserState->GetOriginalChromeBrowserState());
    if (![self.otrTabModel isEmpty]) {
      cookie_util::ClearSessionCookies(
          _mainBrowserState->GetOffTheRecordChromeBrowserState());
    }
  }

  // If the user chooses to restore their session, some cached snapshots may
  // be needed. Otherwise, purge the cached snapshots.
  if (![self mustShowRestoreInfobar]) {
    [self scheduleSnapshotPurge];
  }
}

- (void)scheduleMemoryDebuggingTools {
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    [[DeferredInitializationRunner sharedInstance]
        enqueueBlockNamed:kMemoryDebuggingToolsStartup
                    block:^{
                      _memoryDebuggerManager = [[MemoryDebuggerManager alloc]
                          initWithView:self.window
                                 prefs:GetApplicationContext()
                                           ->GetLocalState()];
                    }];
  }
}

- (void)startFreeMemoryMonitoring {
  // No need for a post-task or a deferred initialisation as the memory
  // monitoring already happens on a background sequence.
  StartFreeMemoryMonitor();
}

- (void)scheduleLowPriorityStartupTasks {
  [_startupTasks initializeOmaha];
  [_startupTasks registerForApplicationWillResignActiveNotification];
  [self registerForOrientationChangeNotifications];

  // Deferred tasks.
  [self schedulePrefObserverInitialization];
  [self scheduleMemoryDebuggingTools];
  [StartupTasks scheduleDeferredBrowserStateInitialization:_mainBrowserState];
  [self scheduleAuthenticationServiceNotification];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteDownloadsDirectory];
  [self scheduleStartupAttemptReset];
  [self startFreeMemoryMonitoring];
  [self scheduleAppDistributionPings];
  [self scheduleCheckForFirstPartyApps];
}

- (void)scheduleTasksRequiringBVCWithBrowserState {
  if (GetApplicationContext()->WasLastShutdownClean()) {
    // Delay the cleanup of the unreferenced files to not impact startup
    // performance.
    ExternalFileRemoverFactory::GetForBrowserState(_mainBrowserState)
        ->RemoveAfterDelay(
            base::TimeDelta::FromSeconds(kExternalFilesCleanupDelaySeconds),
            base::OnceClosure());
  }
  [self scheduleShowPromo];
}

- (void)scheduleDeleteDownloadsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteDownloads
                  block:^{
                    [DownloadManagerController clearDownloadsDirectory];
                  }];
}

- (void)scheduleSpotlightResync {
  if (!_spotlightManager) {
    return;
  }
  ProceduralBlock block = ^{
    [_spotlightManager resyncIndex];
  };
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartSpotlightBookmarksIndexing
                  block:block];
}

- (void)expireFirstUserActionRecorder {
  // Clear out any scheduled calls to this method. For example, the app may have
  // been backgrounded before the |kFirstUserActionTimeout| expired.
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector(
                                                  expireFirstUserActionRecorder)
                                       object:nil];

  if (_firstUserActionRecorder) {
    _firstUserActionRecorder->Expire();
    _firstUserActionRecorder.reset();
  }
}

- (BOOL)canLaunchInIncognito {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  if (![standardDefaults boolForKey:kIncognitoCurrentKey])
    return NO;
  // If the application crashed in incognito mode, don't stay in incognito
  // mode, since the prompt to restore should happen in non-incognito
  // context.
  if ([self mustShowRestoreInfobar])
    return NO;
  // If there are no incognito tabs, then ensure the app starts in normal mode,
  // since the UI isn't supposed to ever put the user in incognito mode without
  // any incognito tabs.
  return ![self.otrTabModel isEmpty];
}

- (void)createInitialUI:(ApplicationMode)launchMode {
  DCHECK(_mainBrowserState);

  // In order to correctly set the mode switch icon, we need to know how many
  // tabs are in the other tab model. That means loading both models.  They
  // may already be loaded.
  // TODO(crbug.com/546203): Find a way to handle this that's closer to the
  // point where it is necessary.
  TabModel* mainTabModel = self.mainTabModel;
  TabModel* otrTabModel = self.otrTabModel;

  // MainCoordinator shouldn't have been initialized yet.
  DCHECK(!_mainCoordinator);

  // Enables UI initializations to query the keyWindow's size.
  [self.window makeKeyAndVisible];

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];

  // Decide if the First Run UI needs to run.
  BOOL firstRun = (FirstRun::IsChromeFirstRun() ||
                   experimental_flags::AlwaysDisplayFirstRun()) &&
                  !tests_hook::DisableFirstRun();

  ios::ChromeBrowserState* browserState =
      (launchMode == ApplicationMode::INCOGNITO)
          ? _mainBrowserState->GetOffTheRecordChromeBrowserState()
          : _mainBrowserState;
  [self changeStorageFromBrowserState:nullptr toBrowserState:browserState];

  TabModel* tabModel;
  if (launchMode == ApplicationMode::INCOGNITO) {
    tabModel = otrTabModel;
    self.currentBVC = self.otrBVC;
  } else {
    tabModel = mainTabModel;
    self.currentBVC = self.mainBVC;
  }
  if (_tabSwitcherIsActive)
    [self dismissTabSwitcherWithoutAnimationInModel:self.mainTabModel];
  if (firstRun || [self shouldOpenNTPTabOnActivationOfTabModel:tabModel]) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:(self.currentBVC == self.otrBVC)];
    command.userInitiated = NO;
    [self.currentBVC.dispatcher openNewTab:command];
  }

  if (firstRun) {
    [self showFirstRunUI];
    // Do not ever show the 'restore' infobar during first run.
    _restoreHelper = nil;
  }
}

- (void)showFirstRunUI {
  // Register for notification when First Run is completed.
  // Some initializations are held back until First Run modal dialog
  // is dismissed.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIWillFinish)
             name:kChromeFirstRunUIWillFinishNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIDidFinish)
             name:kChromeFirstRunUIDidFinishNotification
           object:nil];

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowserState:_mainBrowserState
                      tabModel:self.mainTabModel
                    dispatcher:self.mainBVC.dispatcher];
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  _isPresentingFirstRunUI = YES;
  [self.mainBVC presentViewController:navController animated:NO completion:nil];
}

- (void)crashIfRequested {
  if (experimental_flags::IsStartupCrashEnabled()) {
    // Flush out the value cached for breakpad::SetUploadingEnabled().
    [[NSUserDefaults standardUserDefaults] synchronize];

    int* x = NULL;
    *x = 0;
  }
}

#pragma mark - Promo support

- (void)scheduleShowPromo {
  // Don't show promos if first run is shown.  (Note:  This flag is only YES
  // while the first run UI is visible.  However, as this function is called
  // immediately after the UI is shown, it's a safe check.)
  if (_isPresentingFirstRunUI)
    return;
  // Don't show promos in Incognito mode.
  if (self.currentBVC == self.otrBVC)
    return;
  // Don't show promos if the app was launched from a URL.
  if (_startupParameters)
    return;

  // Show the sign-in promo if needed
  if ([SigninPromoViewController
          shouldBePresentedForBrowserState:_mainBrowserState]) {
    UIViewController* promoController = [[SigninPromoViewController alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(kDisplayPromoDelay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [self showPromo:promoController];
                   });
  }
}

- (void)showPromo:(UIViewController*)promo {
  // Make sure we have the BVC here with a valid profile.
  DCHECK([self.currentBVC browserState]);

  OrientationLimitingNavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:promo];

  // Avoid presenting the promo if the current device orientation is not
  // supported. The promo will be presented at a later moment, when the device
  // orientation is supported.
  UIInterfaceOrientation orientation =
      [UIApplication sharedApplication].statusBarOrientation;
  NSUInteger supportedOrientationsMask =
      [navController supportedInterfaceOrientations];
  if (!((1 << orientation) & supportedOrientationsMask))
    return;

  [navController setModalTransitionStyle:[promo modalTransitionStyle]];
  [navController setNavigationBarHidden:YES];
  [[navController view] setFrame:[[UIScreen mainScreen] bounds]];

  [self.mainBVC presentViewController:navController
                             animated:YES
                           completion:nil];
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
}

- (void)switchModesAndOpenNewTab:(OpenNewTabCommand*)command {
  BrowserViewController* bvc = command.incognito ? self.otrBVC : self.mainBVC;
  DCHECK(bvc);
  [bvc expectNewForegroundTab];
  self.currentBVC = bvc;
  [self.currentBVC.dispatcher openNewTab:command];
}

- (void)startVoiceSearch:(StartVoiceSearchCommand*)command {
  if (!_isProcessingTabSwitcherCommand) {
    [self startVoiceSearchInCurrentBVCWithOriginView:command.originView];
    _isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     _isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)showHistory {
  _historyPanelViewController = [[HistoryPanelViewController alloc]
      initWithLoader:self.currentBVC
        browserState:_mainBrowserState
          dispatcher:self.mainBVC.dispatcher];
  [self.currentBVC presentViewController:_historyPanelViewController
                                animated:YES
                              completion:nil];
}

- (void)closeSettingsUIAndOpenURL:(OpenUrlCommand*)command {
  [self openUrlFromSettings:command];
}

- (void)closeSettingsUI {
  [self closeSettingsAnimated:YES completion:NULL];
}

- (void)displayTabSwitcher {
  DCHECK(!_tabSwitcherIsActive);
  if (!_isProcessingVoiceSearchCommand) {
    [self.currentBVC userEnteredTabSwitcher];
    [self showTabSwitcher];
    _isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     _isProcessingTabSwitcherCommand = NO;
                   });
  }
}

- (void)dismissTabSwitcher {
  if (!IsIPadIdiom()) {
    StackViewController* stackViewController =
        base::mac::ObjCCastStrict<StackViewController>(
            self.tabSwitcherController);
    [stackViewController dismissWithSelectedTabAnimation];
  }
}

- (void)showClearBrowsingDataSettings {
  if (_settingsNavigationController)
    return;
  _settingsNavigationController = [SettingsNavigationController
      newClearBrowsingDataController:_mainBrowserState
                            delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showAutofillSettings {
  if (_settingsNavigationController)
    return;
  _settingsNavigationController =
      [SettingsNavigationController newAutofillController:_mainBrowserState
                                                 delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showSavePasswordsSettings {
  // Do not display password settings if settings screen is already displayed.
  if (_settingsNavigationController)
    return;
  _settingsNavigationController =
      [SettingsNavigationController newSavePasswordsController:_mainBrowserState
                                                      delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showReportAnIssue {
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (_settingsNavigationController)
      return;
    _settingsNavigationController = [SettingsNavigationController
        newUserFeedbackController:_mainBrowserState
                         delegate:self
               feedbackDataSource:self];
    [[self topPresentedViewController]
        presentViewController:_settingsNavigationController
                     animated:YES
                   completion:nil];
  });
}

- (void)openURL:(OpenUrlCommand*)command {
  if ([command fromChrome]) {
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                                        withURL:[command url]
                                 dismissOmnibox:YES
                                     transition:ui::PAGE_TRANSITION_TYPED
                                     completion:nil];
  } else {
    [self dismissModalDialogsWithCompletion:^{
      self.currentBVC = [command inIncognito] ? self.otrBVC : self.mainBVC;
      [self.currentBVC webPageOrderedOpen:[command url]
                                 referrer:[command referrer]
                             inBackground:[command inBackground]
                                 appendTo:[command appendTo]];
    }
                             dismissOmnibox:YES];
  }
}

- (void)showSignin:(ShowSigninCommand*)command {
  if (command.operation == AUTHENTICATION_OPERATION_DISMISS) {
    [self dismissSigninInteractionCoordinator];
  } else {
    [self showSigninWithOperation:command.operation
                         identity:command.identity
                      accessPoint:command.accessPoint
                      promoAction:command.promoAction
                         callback:command.callback];
  }
}

- (void)showAddAccount {
  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];
  }

  [self.signinInteractionCoordinator
      addAccountWithAccessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_UNKNOWN
                    promoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
       presentingViewController:[self topPresentedViewController]
                     completion:nil];
}

#pragma mark - ApplicationSettingsCommands

- (void)showAccountsSettings {
  if ([self currentBrowserState]->IsOffTheRecord()) {
    NOTREACHED();
    return;
  }
  if (_settingsNavigationController) {
    [_settingsNavigationController showAccountsSettings];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newAccountsController:self.currentBrowserState
                   delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showSyncSettings {
  if (_settingsNavigationController) {
    [_settingsNavigationController showSyncSettings];
    return;
  }
  _settingsNavigationController =
      [SettingsNavigationController newSyncController:_mainBrowserState
                               allowSwitchSyncAccount:YES
                                             delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showSyncPassphraseSettings {
  if (_settingsNavigationController) {
    [_settingsNavigationController showSyncPassphraseSettings];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      newSyncEncryptionPassphraseController:_mainBrowserState
                                   delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - chromeExecuteCommand

- (IBAction)chromeExecuteCommand:(id)sender {
  NSInteger command = [sender tag];

  switch (command) {
    case IDC_CLEAR_BROWSING_DATA_IOS: {
      // Clear both the main browser state and the associated incognito
      // browser state.
      ClearBrowsingDataCommand* command =
          base::mac::ObjCCastStrict<ClearBrowsingDataCommand>(sender);
      ios::ChromeBrowserState* browserState =
          [command browserState]->GetOriginalChromeBrowserState();
      int mask = [command mask];
      browsing_data::TimePeriod timePeriod = [command timePeriod];
      [self removeBrowsingDataFromBrowserState:browserState
                                          mask:mask
                                    timePeriod:timePeriod
                             completionHandler:nil];

      if (mask & IOSChromeBrowsingDataRemover::REMOVE_COOKIES) {
        base::Time beginDeleteTime =
            browsing_data::CalculateBeginDeleteTime(timePeriod);
        [ChromeWebViewFactory clearExternalCookies:browserState
                                          fromTime:beginDeleteTime
                                            toTime:base::Time::Max()];
      }
      break;
    }
    default:
      // Unknown commands get dropped with a warning.
      NOTREACHED() << "Unknown command id " << command;
      LOG(WARNING) << "Unknown command id " << command;
      break;
  }
}

#pragma mark - chromeExecuteCommand helpers

- (void)openUrlFromSettings:(OpenUrlCommand*)command {
  DCHECK([command fromChrome]);
  ProceduralBlock completion = ^{
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                                        withURL:[command url]
                                 dismissOmnibox:YES
                                     transition:ui::PAGE_TRANSITION_TYPED
                                     completion:nil];
  };
  [self closeSettingsAnimated:YES completion:completion];
}

- (void)startVoiceSearchInCurrentBVCWithOriginView:(UIView*)originView {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  BrowserViewController* backgroundBVC =
      self.mainBVC == self.currentBVC ? self.otrBVC : self.mainBVC;
  if (backgroundBVC.playingTTS)
    [backgroundBVC startVoiceSearchWithOriginView:originView];
  else
    [self.currentBVC startVoiceSearchWithOriginView:originView];
}

#pragma mark - Preferences Management

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Turn on or off metrics & crash reporting when either preference changes.
  if (preferenceName == metrics::prefs::kMetricsReportingEnabled ||
      preferenceName == prefs::kMetricsReportingWifiOnly) {
    [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:YES];
  }
}

#pragma mark - BrowserViewInformation properties

- (BrowserViewController*)mainBVC {
  DCHECK(_browserViewWrangler);
  return [_browserViewWrangler mainBVC];
}

- (void)setMainBVC:(BrowserViewController*)mainBVC {
  DCHECK(_browserViewWrangler);
  [_browserViewWrangler setMainBVC:mainBVC];
}

- (TabModel*)mainTabModel {
  DCHECK(_browserViewWrangler);
  return [_browserViewWrangler mainTabModel];
}

- (void)setMainTabModel:(TabModel*)mainTabModel {
  DCHECK(_browserViewWrangler);
  [_browserViewWrangler setMainTabModel:mainTabModel];
}

- (BrowserViewController*)otrBVC {
  DCHECK(_browserViewWrangler);
  return [_browserViewWrangler otrBVC];
}

- (void)setOtrBVC:(BrowserViewController*)otrBVC {
  DCHECK(_browserViewWrangler);
  [_browserViewWrangler setOtrBVC:otrBVC];
}

- (TabModel*)otrTabModel {
  DCHECK(_browserViewWrangler);
  return [_browserViewWrangler otrTabModel];
}

- (void)setOtrTabModel:(TabModel*)otrTabModel {
  DCHECK(_browserViewWrangler);
  [_browserViewWrangler setOtrTabModel:otrTabModel];
}

- (BrowserViewController*)currentBVC {
  DCHECK(_browserViewWrangler);
  return [_browserViewWrangler currentBVC];
}

// Note that the current tab of |bvc| will normally be reloaded by this method.
// If a new tab is about to be added, call expectNewForegroundTab on the BVC
// first to avoid extra work and possible page load side-effects for the tab
// being replaced.
- (void)setCurrentBVC:(BrowserViewController*)bvc {
  DCHECK(bvc != nil);
  if (self.currentBVC == bvc)
    return;

  DCHECK(_browserViewWrangler);
  [_browserViewWrangler setCurrentBVC:bvc storageSwitcher:self];

  if (!_dismissingStackView)
    [self displayCurrentBVC];

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
}

#pragma mark - Tab closure handlers

- (void)lastIncognitoTabClosed {
  DCHECK(_mainBrowserState->HasOffTheRecordChromeBrowserState());
  [self clearIOSSpecificIncognitoData];

  // OffTheRecordProfileIOData cannot be deleted before all the requests are
  // deleted. All of the request trackers associated with the closed OTR tabs
  // will have posted CancelRequest calls to the IO thread by now; this just
  // waits for those calls to run before calling |deleteIncognitoBrowserState|.
  web::RequestTrackerImpl::RunAfterRequestsCancel(base::BindBlockArc(^{
    [self deleteIncognitoBrowserState];
  }));

  // a) The first condition can happen when the last incognito tab is closed
  // from the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the otr tab model when it's not current.
  // Nothing to do here. The next user action (like clicking on an existing
  // regular tab or creating a new incognito tab from the settings menu) will
  // take care of the logic to mode switch.
  if (_tabSwitcherIsActive || ![self.currentTabModel isOffTheRecord]) {
    return;
  }

  if (IsIPadIdiom()) {
    [self showTabSwitcher];
  } else {
    self.currentBVC = self.mainBVC;
    if ([self.currentTabModel count] == 0U) {
      [self showTabSwitcher];
    }
  }
}

- (void)lastRegularTabClosed {
  // a) The first condition can happen when the last regular tab is closed from
  // the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the main tab model when the main tab model is not
  // current.
  // Nothing to do here.
  if (_tabSwitcherIsActive || [self.currentTabModel isOffTheRecord]) {
    return;
  }

  [self showTabSwitcher];
}

#pragma mark - Mode Switching

- (void)switchGlobalStateToMode:(ApplicationMode)mode {
  const BOOL incognito = (mode == ApplicationMode::INCOGNITO);
  // Write the state to disk of what is "active".
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:incognito forKey:kIncognitoCurrentKey];
  // Save critical state information for switching between normal and
  // incognito.
  [standardDefaults synchronize];
}

- (void)changeStorageFromBrowserState:(ios::ChromeBrowserState*)oldState
                       toBrowserState:(ios::ChromeBrowserState*)newState {
  ApplicationMode mode = newState->IsOffTheRecord() ? ApplicationMode::INCOGNITO
                                                    : ApplicationMode::NORMAL;
  [self switchGlobalStateToMode:mode];
}

- (void)displayCurrentBVC {
  [self.mainViewController showTabViewController:self.currentBVC
                                      completion:nil];
}

- (TabModel*)currentTabModel {
  return self.currentBVC.tabModel;
}

- (ios::ChromeBrowserState*)currentBrowserState {
  return self.currentBVC.browserState;
}

// NOTE: If you change this function, it may have an effect on the performance
// of opening the stack view. Please make sure you also change the corresponding
// code in StackViewControllerPerfTest::MainControllerShowTabSwitcher().
- (void)showTabSwitcher {
  BrowserViewController* currentBVC = self.currentBVC;
  Tab* currentTab = [[currentBVC tabModel] currentTab];

  // In order to generate the transition between the current browser view
  // controller and the tab switcher controller it's possible that multiple
  // screenshots of the same tab are taken. Since taking a screenshot is
  // expensive we activate snapshot coalescing in the scope of this function
  // which will cache the first snapshot for the tab and reuse it instead of
  // regenerating a new one each time.
  [currentTab setSnapshotCoalescingEnabled:YES];
  base::ScopedClosureRunner runner(base::BindBlockArc(^{
    [currentTab setSnapshotCoalescingEnabled:NO];
  }));

  [currentTab updateSnapshotWithOverlay:YES visibleFrameOnly:YES];

  if (!_tabSwitcherController) {
    if (IsIPadIdiom()) {
      _tabSwitcherController = [[TabSwitcherController alloc]
                initWithBrowserState:_mainBrowserState
                        mainTabModel:self.mainTabModel
                         otrTabModel:self.otrTabModel
                      activeTabModel:self.currentTabModel
          applicationCommandEndpoint:self];
    } else {
      _tabSwitcherController =
          [[StackViewController alloc] initWithMainTabModel:self.mainTabModel
                                                otrTabModel:self.otrTabModel
                                             activeTabModel:self.currentTabModel
                                 applicationCommandEndpoint:self];
    }
  } else {
    // The StackViewController is kept in memory to avoid the performance hit of
    // loading from the nib on next showing, but clears out its card models to
    // release memory.  The tab models are required to rebuild the card stacks.
    [_tabSwitcherController
        restoreInternalStateWithMainTabModel:self.mainTabModel
                                 otrTabModel:self.otrTabModel
                              activeTabModel:self.currentTabModel];
  }
  _tabSwitcherIsActive = YES;
  [_tabSwitcherController setDelegate:self];
  if (IsIPadIdiom()) {
    TabSwitcherTransitionContext* transitionContext =
        [TabSwitcherTransitionContext
            tabSwitcherTransitionContextWithCurrent:currentBVC
                                            mainBVC:self.mainBVC
                                             otrBVC:self.otrBVC];
    [_tabSwitcherController setTransitionContext:transitionContext];
  } else {
    // User interaction is disabled when the stack controller is dismissed.
    [[_tabSwitcherController view] setUserInteractionEnabled:YES];
  }

  [self.mainViewController showTabSwitcher:_tabSwitcherController
                                completion:nil];
}

- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel {
  if (_settingsNavigationController) {
    return false;
  }
  if (_tabSwitcherIsActive) {
    // Only attempt to dismiss the tab switcher and open a new tab if:
    // - there are no tabs open in either tab model, and
    // - the tab switcher controller is not directly or indirectly presenting
    // another view controller.
    if (![self.mainTabModel isEmpty] || ![self.otrTabModel isEmpty])
      return NO;

    UIViewController* viewController = [self topPresentedViewController];
    while (viewController) {
      if ([viewController.presentingViewController
              isEqual:_tabSwitcherController]) {
        return NO;
      }
      viewController = viewController.presentingViewController;
    }
    return YES;
  }
  return ![tabModel count] && [tabModel browserState] &&
         ![tabModel browserState]->IsOffTheRecord();
}

#pragma mark - TabSwitching implementation.

- (BOOL)openNewTabFromTabSwitcher {
  if (!_tabSwitcherController)
    return NO;

  [_tabSwitcherController
      dismissWithNewTabAnimationToModel:self.mainTabModel
                                withURL:GURL(kChromeUINewTabURL)
                                atIndex:NSNotFound
                             transition:ui::PAGE_TRANSITION_TYPED];
  return YES;
}

- (void)dismissTabSwitcherWithoutAnimationInModel:(TabModel*)tabModel {
  DCHECK(_tabSwitcherIsActive);
  DCHECK(!_dismissingStackView);
  if ([_tabSwitcherController
          respondsToSelector:@selector(tabSwitcherDismissWithModel:
                                                          animated:)]) {
    [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
    [_tabSwitcherController tabSwitcherDismissWithModel:tabModel animated:NO];
  } else {
    [self beginDismissingStackViewWithCurrentModel:tabModel];
    [self finishDismissingStackView];
  }
}

#pragma mark - TabSwitcherDelegate Implementation

- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    dismissTransitionWillStartWithActiveModel:(TabModel*)tabModel {
  [self beginDismissingStackViewWithCurrentModel:tabModel];
}

- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  [self finishDismissingStackView];
}

- (void)beginDismissingStackViewWithCurrentModel:(TabModel*)tabModel {
  DCHECK(tabModel == self.mainTabModel || tabModel == self.otrTabModel);

  _dismissingStackView = YES;
  // Prevent wayward touches from wreaking havoc while the stack view is being
  // dismissed.
  [[_tabSwitcherController view] setUserInteractionEnabled:NO];
  BrowserViewController* targetBVC =
      (tabModel == self.mainTabModel) ? self.mainBVC : self.otrBVC;
  self.currentBVC = targetBVC;
}

- (void)finishDismissingStackView {
  DCHECK_EQ(self.mainViewController.activeViewController,
            _tabSwitcherController);

  if (_modeToDisplayOnStackViewDismissal == StackViewDismissalMode::NORMAL) {
    self.currentBVC = self.mainBVC;
  } else if (_modeToDisplayOnStackViewDismissal ==
             StackViewDismissalMode::INCOGNITO) {
    self.currentBVC = self.otrBVC;
  }

  _modeToDisplayOnStackViewDismissal = StackViewDismissalMode::NONE;

  // Displaying the current BVC dismisses the stack view.
  [self displayCurrentBVC];

  ProceduralBlock action = [self completionBlockForTriggeringAction:
                                     self.NTPActionAfterTabSwitcherDismissal];
  self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
  if (action) {
    action();
  }

  [_tabSwitcherController setDelegate:nil];

  _tabSwitcherIsActive = NO;
  _dismissingStackView = NO;
}

- (void)tabSwitcherPresentationTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
}

- (id<ToolbarOwner>)tabSwitcherTransitionToolbarOwner {
  // Request the view to ensure that the view has been loaded and initialized,
  // since it may never have been loaded (or have been swapped out).
  [self.currentBVC loadViewIfNeeded];
  return self.currentBVC;
}

#pragma mark - Browsing data clearing

- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(int)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler {
  // TODO(crbug.com/632772): Remove web usage disabling once
  // https://bugs.webkit.org/show_bug.cgi?id=149079 has been fixed.
  if (mask & IOSChromeBrowsingDataRemover::REMOVE_SITE_DATA) {
    [self setWebUsageEnabled:NO];
  }
  ProceduralBlock browsingDataRemoved = ^{
    [self setWebUsageEnabled:YES];
    if (completionHandler) {
      completionHandler();
    }
  };
  [self.browsingDataRemovalController
      removeBrowsingDataFromBrowserState:browserState
                                    mask:mask
                              timePeriod:timePeriod
                       completionHandler:browsingDataRemoved];
}

#pragma mark - Navigation Controllers

- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  UIViewController* accountsViewController =
      [[SignedInAccountsViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:self.mainBVC.dispatcher];
  [[self topPresentedViewController]
      presentViewController:accountsViewController
                   animated:YES
                 completion:nil];
}

- (void)showSettings {
  if (_settingsNavigationController)
    return;
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];
  DCHECK(_localStatePrefObserverBridge);
  _settingsNavigationController = [SettingsNavigationController
      newSettingsMainControllerWithBrowserState:_mainBrowserState
                                       delegate:self];
  [[self topPresentedViewController]
      presentViewController:_settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showSigninWithOperation:(AuthenticationOperation)operation
                       identity:(ChromeIdentity*)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
                    promoAction:(signin_metrics::PromoAction)promoAction
                       callback:(ShowSigninCommandCompletionCallback)callback {
  DCHECK_NE(AUTHENTICATION_OPERATION_DISMISS, operation);

  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator = [[SigninInteractionCoordinator alloc]
        initWithBrowserState:_mainBrowserState
                  dispatcher:self.mainBVC.dispatcher];
  }

  switch (operation) {
    case AUTHENTICATION_OPERATION_DISMISS:
      // Special case handled above.
      NOTREACHED();
      break;
    case AUTHENTICATION_OPERATION_REAUTHENTICATE:
      [self.signinInteractionCoordinator
          reAuthenticateWithAccessPoint:accessPoint
                            promoAction:promoAction
               presentingViewController:[self topPresentedViewController]
                             completion:callback];
      break;
    case AUTHENTICATION_OPERATION_SIGNIN:
      [self.signinInteractionCoordinator
                signInWithIdentity:identity
                       accessPoint:accessPoint
                       promoAction:promoAction
          presentingViewController:[self topPresentedViewController]
                        completion:callback];
      break;
  }
}

- (void)dismissSigninInteractionCoordinator {
  // The SigninInteractionCoordinator must not be destroyed at this point, as
  // it may dismiss the sign in UI in a future callback.
  [self.signinInteractionCoordinator cancelAndDismiss];
}

- (void)closeSettingsAnimated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  DCHECK(_settingsNavigationController);
  [_settingsNavigationController settingsWillBeDismissed];
  UIViewController* presentingViewController =
      [_settingsNavigationController presentingViewController];
  DCHECK(presentingViewController);
  [presentingViewController dismissViewControllerAnimated:animated
                                               completion:^{
                                                 if (completion)
                                                   completion();
                                               }];
  _settingsNavigationController = nil;
}

#pragma mark - TabModelObserver

// Called when the number of tabs changes. Triggers the switcher view when
// the last tab is closed on a device that uses the switcher.
- (void)tabModelDidChangeTabCount:(TabModel*)notifiedTabModel {
  TabModel* currentTabModel = [self currentTabModel];
  // Do nothing on initialization.
  if (!currentTabModel)
    return;

  if (notifiedTabModel.count == 0U) {
    if ([notifiedTabModel isOffTheRecord]) {
      [self lastIncognitoTabClosed];
    } else {
      [self lastRegularTabClosed];
    }
  }
}

#pragma mark - Tab opening utility methods.

- (Tab*)openOrReuseTabInMode:(ApplicationMode)targetMode
                     withURL:(const GURL&)URL
                  transition:(ui::PageTransition)transition
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  BrowserViewController* targetBVC =
      targetMode == ApplicationMode::NORMAL ? self.mainBVC : self.otrBVC;

  Tab* currentTabInTargetBVC = [[targetBVC tabModel] currentTab];
  if (!(currentTabInTargetBVC.webState &&
        IsURLNtp(currentTabInTargetBVC.webState->GetVisibleURL()))) {
    return [targetBVC addSelectedTabWithURL:URL
                                    atIndex:NSNotFound
                                 transition:transition
                         tabAddedCompletion:tabOpenedCompletion];
  }

  Tab* newTab = currentTabInTargetBVC;
  // Don't call loadWithParams for chrome://newtab, it's already loaded.
  if (!(IsURLNtp(URL))) {
    web::NavigationManager::WebLoadParams params(URL);
    [newTab webState]->GetNavigationManager()->LoadURLWithParams(params);
  }
  if (tabOpenedCompletion) {
    tabOpenedCompletion();
  }
  return newTab;
}

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action {
  switch (action) {
    case START_VOICE_SEARCH:
      if (@available(iOS 11, *)) {
        return ^{
          [self startVoiceSearchInCurrentBVCWithOriginView:nil];
        };
      } else {
        return ^{
          // On iOS10.3.X, the launching the application using an external URL
          // sometimes triggers notifications
          // applicationDidBecomeActive
          // applicationWillResignActive
          // applicationDidBecomeActive.
          // Triggering voiceSearch immediatley will cause its dismiss on
          // applicationWillResignActive.
          // Add a timer here and hope this will be enough so that voice search
          // is triggered after second applicationDidBecomeActive.
          // TODO(crbug.com/766951): remove this workaround.
          dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC),
                         dispatch_get_main_queue(), ^{
                           [self
                               startVoiceSearchInCurrentBVCWithOriginView:nil];
                         });

        };
      }
    case START_QR_CODE_SCANNER:
      return ^{
        [self.currentBVC.dispatcher showQRScanner];
      };
    case FOCUS_OMNIBOX:
      return ^{
        [self.currentBVC focusOmnibox];
      };
    default:
      return nil;
  }
}

- (Tab*)openSelectedTabInMode:(ApplicationMode)targetMode
                      withURL:(const GURL&)url
                   transition:(ui::PageTransition)transition
                   completion:(ProceduralBlock)completion {
  BrowserViewController* targetBVC =
      targetMode == ApplicationMode::NORMAL ? self.mainBVC : self.otrBVC;
  NSUInteger tabIndex = NSNotFound;

  ProceduralBlock startupCompletion =
      [self completionBlockForTriggeringAction:[_startupParameters
                                                   postOpeningAction]];
  // Commands are only allowed on NTP.
  DCHECK(IsURLNtp(url) || !startupCompletion);

  ProceduralBlock tabOpenedCompletion = nil;
  if (startupCompletion && completion) {
    tabOpenedCompletion = ^{
      // Order is important here. |completion| may do cleaning tasks that will
      // invalidate |startupCompletion|.
      startupCompletion();
      completion();
    };
  } else if (startupCompletion) {
    tabOpenedCompletion = startupCompletion;
  } else {
    tabOpenedCompletion = completion;
  }

  Tab* tab = nil;
  if (_tabSwitcherIsActive) {
    // If the stack view is already being dismissed, simply add the tab and
    // note that when the stack view finishes dismissing, the current BVC should
    // be switched to be the main BVC if necessary.
    if (_dismissingStackView) {
      _modeToDisplayOnStackViewDismissal =
          targetMode == ApplicationMode::NORMAL
              ? StackViewDismissalMode::NORMAL
              : StackViewDismissalMode::INCOGNITO;
      tab = [targetBVC addSelectedTabWithURL:url
                                     atIndex:tabIndex
                                  transition:transition
                          tabAddedCompletion:tabOpenedCompletion];
    } else {
      // Voice search, QRScanner and the omnibox are presented by the BVC.
      // They must be started after the BVC view is added in the hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [_startupParameters postOpeningAction];
      [self setStartupParameters:nil];
      tab = [_tabSwitcherController
          dismissWithNewTabAnimationToModel:targetBVC.tabModel
                                    withURL:url
                                    atIndex:tabIndex
                                 transition:transition];
    }
  } else {
    if (!self.currentBVC.presentedViewController) {
      [targetBVC expectNewForegroundTab];
    }
    self.currentBVC = targetBVC;
    tab = [self openOrReuseTabInMode:targetMode
                             withURL:url
                          transition:transition
                 tabOpenedCompletion:tabOpenedCompletion];
  }

  if (_restoreHelper) {
    // Now that all the operations on the tabs have been done, display the
    // restore infobar if needed.
    dispatch_async(dispatch_get_main_queue(), ^{
      [_restoreHelper showRestoreIfNeeded:[self currentTabModel]];
      _restoreHelper = nil;
    });
  }

  return tab;
}

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  // Immediately hide modals from the provider (alert views, action sheets,
  // popovers). They will be ultimately dismissed by their owners, but at least,
  // they are not visible.
  ios::GetChromeBrowserProvider()->HideModalViewStack();

  // ChromeIdentityService is responsible for the dialogs displayed by the
  // services it wraps.
  ios::GetChromeBrowserProvider()->GetChromeIdentityService()->DismissDialogs();

  // Cancel interaction with SSO.
  // First, cancel the signin interaction.
  [self.signinInteractionCoordinator cancel];

  // Then, depending on what the SSO view controller is presented on, dismiss
  // it.
  ProceduralBlock completionWithBVC = ^{
    // This will dismiss the SSO view controller.
    [self.currentBVC clearPresentedStateWithCompletion:completion
                                        dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // This will dismiss the SSO view controller.
    [self dismissSigninInteractionCoordinator];
    if (completion)
      completion();
  };

  // As a top level rule, if the settings are showing, they need to be
  // dismissed. Then, based on whether the BVC is present or not, a different
  // completion callback is called.
  if (self.currentBVC && _settingsNavigationController) {
    // In this case, the settings are up and the BVC is showing. Close the
    // settings then call the BVC completion.
    [self closeSettingsAnimated:NO completion:completionWithBVC];
  } else if (_settingsNavigationController) {
    // In this case, the settings are up but the BVC is not showing. Close the
    // settings then call the no-BVC completion.
    [self closeSettingsAnimated:NO completion:completionWithoutBVC];
  } else if (self.currentBVC) {
    // In this case, the settings are not shown but the BVC is showing. Call the
    // BVC completion.
    completionWithBVC();
  } else {
    // In this case, neither the settings nor the BVC are shown. Call the no-BVC
    // completion.
    completionWithoutBVC();
  }

  // Verify that no modal views are left presented.
  ios::GetChromeBrowserProvider()->LogIfModalViewsArePresented();
}

// iOS does not guarantee the order in which the observers are notified by
// the notification center. There are different parts of the application that
// register for UIApplication notifications so recording them in order to
// measure the performance of the app being moved to the foreground / background
// is not reliable. Instead we prefer using designated notifications that are
// posted to the observers on the first available run loop cycle, which
// guarantees that they are delivered to the observer only after UIApplication
// notifications have been treated.
- (void)postNotificationOnNextRunLoopCycle:(NSString*)notificationName {
  dispatch_async(dispatch_get_main_queue(), ^{
    [[NSNotificationCenter defaultCenter] postNotificationName:notificationName
                                                        object:self];
  });
}

- (bool)mustShowRestoreInfobar {
  if ([self isFirstLaunchAfterUpgrade])
    return false;
  return !GetApplicationContext()->WasLastShutdownClean();
}

- (NSMutableSet*)liveSessionsForTabModel:(TabModel*)tabModel {
  NSMutableSet* result = [NSMutableSet setWithCapacity:[tabModel count]];
  for (Tab* tab in tabModel) {
    [result addObject:tab.tabId];
  }
  return result;
}

- (void)purgeSnapshots {
  NSMutableSet* liveSessions = [self liveSessionsForTabModel:self.mainTabModel];
  [liveSessions unionSet:[self liveSessionsForTabModel:self.otrTabModel]];

  // Keep snapshots that are less than one minute old, to prevent a concurrency
  // issue if they are created while the purge is running.
  const base::Time oneMinuteAgo =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);
  [SnapshotCacheFactory::GetForBrowserState([self currentBrowserState])
      purgeCacheOlderThan:oneMinuteAgo
                  keeping:liveSessions];
}

- (void)markEulaAsAccepted {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  if (!prefs->GetBoolean(prefs::kEulaAccepted))
    prefs->SetBoolean(prefs::kEulaAccepted, true);
  prefs->CommitPendingWrite();
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndOpenSelectedTabInMode:(ApplicationMode)targetMode
                                      withURL:(const GURL&)url
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   transition:(ui::PageTransition)transition
                                   completion:(ProceduralBlock)completion {
  GURL copyOfURL = url;
  [self dismissModalDialogsWithCompletion:^{
    [self openSelectedTabInMode:targetMode
                        withURL:copyOfURL
                     transition:transition
                     completion:completion];
  }
                           dismissOmnibox:dismissOmnibox];
}

- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState {
  if (launchOptions) {
    BOOL applicationIsActive =
        [[UIApplication sharedApplication] applicationState] ==
        UIApplicationStateActive;

    [URLOpener handleLaunchOptions:launchOptions
                 applicationActive:applicationIsActive
                         tabOpener:self
                startupInformation:startupInformation
                          appState:appState];
  }
}

- (BOOL)shouldCompletePaymentRequestOnCurrentTab:
    (id<StartupInformation>)startupInformation {
  if (!startupInformation.startupParameters)
    return NO;

  if (!startupInformation.startupParameters.completePaymentRequest)
    return NO;

  if (!base::FeatureList::IsEnabled(payments::features::kWebPaymentsNativeApps))
    return NO;

  payments::IOSPaymentInstrumentLauncher* paymentAppLauncher =
      payments::IOSPaymentInstrumentLauncherFactory::GetInstance()
          ->GetForBrowserState(_mainBrowserState);

  if (!paymentAppLauncher->delegate())
    return NO;

  std::string payment_id =
      startupInformation.startupParameters.externalURLParams
          .find(payments::kPaymentRequestIDExternal)
          ->second;
  if (paymentAppLauncher->payment_request_id() != payment_id)
    return NO;

  std::string payment_response =
      startupInformation.startupParameters.externalURLParams
          .find(payments::kPaymentRequestDataExternal)
          ->second;
  paymentAppLauncher->ReceiveResponseFromIOSPaymentInstrument(payment_response);
  [startupInformation setStartupParameters:nil];
  return YES;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)closeSettingsAndOpenNewIncognitoTab {
  [self closeSettingsAnimated:NO
                   completion:^{
                     [self switchModesAndOpenNewTab:[OpenNewTabCommand
                                                        incognitoTabCommand]];
                   }];
}

- (id<ApplicationCommands, BrowserCommands>)dispatcherForSettings {
  // Assume that settings always wants the dispatcher from the main BVC.
  return self.mainBVC.dispatcher;
}

#pragma mark - UserFeedbackDataSource

- (NSString*)currentPageDisplayURL {
  if (_tabSwitcherIsActive)
    return nil;
  web::WebState* webState = [[[self currentTabModel] currentTab] webState];
  if (!webState)
    return nil;
  // Returns URL of browser tab that is currently showing.
  GURL url = webState->GetVisibleURL();
  base::string16 urlText = url_formatter::FormatUrl(url);
  return base::SysUTF16ToNSString(urlText);
}

- (UIImage*)currentPageScreenshot {
  UIView* lastView = self.mainViewController.view;
  DCHECK(lastView);
  CGFloat scale = 0.0;
  // For screenshots of the Stack View we need to use a scale of 1.0 to avoid
  // spending too much time since the Stack View can have lots of subviews.
  if (_tabSwitcherIsActive)
    scale = 1.0;
  return CaptureView(lastView, scale);
}

- (BOOL)currentPageIsIncognito {
  return [self currentBrowserState]->IsOffTheRecord();
}

- (NSString*)currentPageSyncedUserName {
  ios::ChromeBrowserState* browserState = [self currentBrowserState];
  if (browserState->IsOffTheRecord())
    return nil;
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browserState);
  std::string username = signin_manager->GetAuthenticatedAccountInfo().email;
  return username.empty() ? nil : base::SysUTF8ToNSString(username);
}

#pragma mark - UI Automation Testing

- (void)setUpCurrentBVCForTesting {
  // Notify that the set up will close all tabs.
  if (!IsIPadIdiom()) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSetupForTestingWillCloseAllTabsNotification
                      object:self];
  }

  [self.otrTabModel closeAllTabs];
  [self.mainTabModel closeAllTabs];
}

@end

#pragma mark - TestingOnly

@implementation MainController (TestingOnly)

- (DeviceSharingManager*)deviceSharingManager {
  return [_browserViewWrangler deviceSharingManager];
}

- (UIViewController<TabSwitcher>*)tabSwitcherController {
  return _tabSwitcherController;
}

- (void)setTabSwitcherController:(UIViewController<TabSwitcher>*)controller {
  _tabSwitcherController = controller;
}

- (UIViewController*)topPresentedViewController {
  // TODO(crbug.com/754642): Implement TopPresentedViewControllerFrom()
  // privately.
  return top_view_controller::TopPresentedViewControllerFrom(
      self.mainViewController);
}

- (void)setTabSwitcherActive:(BOOL)active {
  _tabSwitcherIsActive = active;
}

- (BOOL)dismissingTabSwitcher {
  return _dismissingStackView;
}

- (void)setStartupParametersWithURL:(const GURL&)launchURL {
  NSString* sourceApplication = @"Fake App";
  _startupParameters = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:net::NSURLWithGURL(launchURL)
                     fromSourceApplication:sourceApplication];
}

- (void)setUpAsForegrounded {
  _isColdStart = NO;
  _browserInitializationStage = INITIALIZATION_STAGE_FOREGROUND;
  // Create a BrowserViewWrangler with a null browser state. This will trigger
  // assertions if the BrowserViewWrangler is asked to create any BVC or
  // tabModel objects, but it will accept assignments to them.
  [_browserViewWrangler shutdown];
  _browserViewWrangler =
      [[BrowserViewWrangler alloc] initWithBrowserState:nullptr
                                       tabModelObserver:self
                             applicationCommandEndpoint:self];
  // This is a test utility method that bypasses the ususal setup steps, so
  // verify that the main coordinator hasn't been created yet, then start it
  // via lazy initialization.
  DCHECK(!_mainCoordinator);
  [self.mainCoordinator start];
}

- (void)setUpForTestingWithCompletionHandler:
    (ProceduralBlock)completionHandler {
  self.currentBVC = self.mainBVC;

  int removeAllMask = ~0;
  scoped_refptr<CallbackCounter> callbackCounter =
      new CallbackCounter(base::BindBlockArc(^{
        [self setUpCurrentBVCForTesting];
        if (completionHandler) {
          completionHandler();
        }
      }));
  id decrementCallbackCounterCount = ^{
    callbackCounter->DecrementCount();
  };

  callbackCounter->IncrementCount();
  [self removeBrowsingDataFromBrowserState:_mainBrowserState
                                      mask:removeAllMask
                                timePeriod:browsing_data::TimePeriod::ALL_TIME
                         completionHandler:decrementCallbackCounterCount];
}

@end
