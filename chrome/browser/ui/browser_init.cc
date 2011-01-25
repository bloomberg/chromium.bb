// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_init.h"

#include <algorithm>   // For max().

#include "base/environment.h"
#include "base/event_recorder.h"
#include "base/file_path.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/chrome_frame_automation_provider.h"
#include "chrome/browser/automation/testing_automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/keystone_infobar.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/brightness_observer.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/chromeos/low_battery_observer.h"
#include "chrome/browser/chromeos/network_message_observer.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/chromeos/update_observer.h"
#include "chrome/browser/chromeos/usb_mount_observer.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "chrome/browser/chromeos/wm_overview_controller.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#endif

#if defined(HAVE_XINPUT2)
#include "views/focus/accelerator_handler.h"
#endif

namespace {

// SetAsDefaultBrowserTask ----------------------------------------------------

class SetAsDefaultBrowserTask : public Task {
 public:
  SetAsDefaultBrowserTask();
  virtual ~SetAsDefaultBrowserTask();

 private:
  virtual void Run();

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserTask);
};

SetAsDefaultBrowserTask::SetAsDefaultBrowserTask() {
}

SetAsDefaultBrowserTask::~SetAsDefaultBrowserTask() {
}

void SetAsDefaultBrowserTask::Run() {
  ShellIntegration::SetAsDefaultBrowser();
}


// DefaultBrowserInfoBarDelegate ----------------------------------------------

// The delegate for the infobar shown when Chrome is not the default browser.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit DefaultBrowserInfoBarDelegate(TabContents* contents);

 private:
  virtual ~DefaultBrowserInfoBarDelegate();

  void AllowExpiry() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool NeedElevation(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

 private:
  // The Profile that we restore sessions from.
  Profile* profile_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  ScopedRunnableMethodFactory<DefaultBrowserInfoBarDelegate> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegate);
};

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(
    TabContents* contents)
    : ConfirmInfoBarDelegate(contents),
      profile_(contents->profile()),
      action_taken_(false),
      should_expire_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &DefaultBrowserInfoBarDelegate::AllowExpiry), 8000);  // 8 seconds.
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  return should_expire_;
}

void DefaultBrowserInfoBarDelegate::InfoBarClosed() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.Ignored", 1);
  delete this;
}

SkBitmap* DefaultBrowserInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
     IDR_PRODUCT_ICON_32);
}

string16 DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
}

int DefaultBrowserInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return button == BUTTON_OK ?
      l10n_util::GetStringUTF16(IDS_SET_AS_DEFAULT_INFOBAR_BUTTON_LABEL) :
      l10n_util::GetStringUTF16(IDS_DONT_ASK_AGAIN_INFOBAR_BUTTON_LABEL);
}

bool DefaultBrowserInfoBarDelegate::NeedElevation(InfoBarButton button) const {
  return button == BUTTON_OK;
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.SetAsDefault", 1);
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new SetAsDefaultBrowserTask());
  return true;
}

bool DefaultBrowserInfoBarDelegate::Cancel() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.DontSetAsDefault", 1);
  // User clicked "Don't ask me again", remember that.
  profile_->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, false);
  return true;
}


// NotifyNotDefaultBrowserTask ------------------------------------------------

class NotifyNotDefaultBrowserTask : public Task {
 public:
  NotifyNotDefaultBrowserTask();
  virtual ~NotifyNotDefaultBrowserTask();

 private:
  virtual void Run();

  DISALLOW_COPY_AND_ASSIGN(NotifyNotDefaultBrowserTask);
};

NotifyNotDefaultBrowserTask::NotifyNotDefaultBrowserTask() {
}

NotifyNotDefaultBrowserTask::~NotifyNotDefaultBrowserTask() {
}

void NotifyNotDefaultBrowserTask::Run() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;  // Reached during ui tests.
  // Don't show the info-bar if there are already info-bars showing.
  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  TabContents* tab = browser->GetSelectedTabContents();
  if (!tab || tab->infobar_delegate_count() > 0)
    return;
  tab->AddInfoBar(new DefaultBrowserInfoBarDelegate(tab));
}


// CheckDefaultBrowserTask ----------------------------------------------------

class CheckDefaultBrowserTask : public Task {
 public:
  CheckDefaultBrowserTask();
  virtual ~CheckDefaultBrowserTask();

 private:
  virtual void Run();

  DISALLOW_COPY_AND_ASSIGN(CheckDefaultBrowserTask);
};

CheckDefaultBrowserTask::CheckDefaultBrowserTask() {
}

CheckDefaultBrowserTask::~CheckDefaultBrowserTask() {
}

void CheckDefaultBrowserTask::Run() {
  if (ShellIntegration::IsDefaultBrowser())
    return;
#if defined(OS_WIN)
  if (!BrowserDistribution::GetDistribution()->CanSetAsDefault())
    return;
#endif
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          new NotifyNotDefaultBrowserTask());
}


// SessionCrashedInfoBarDelegate ----------------------------------------------

// A delegate for the InfoBar shown when the previous session has crashed. The
// bar deletes itself automatically after it is closed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit SessionCrashedInfoBarDelegate(TabContents* contents);

 private:
  virtual ~SessionCrashedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();

  // The Profile that we restore sessions from.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

SessionCrashedInfoBarDelegate::SessionCrashedInfoBarDelegate(
    TabContents* contents)
    : ConfirmInfoBarDelegate(contents),
      profile_(contents->profile()) {
}

SessionCrashedInfoBarDelegate::~SessionCrashedInfoBarDelegate() {
}

void SessionCrashedInfoBarDelegate::InfoBarClosed() {
  delete this;
}

SkBitmap* SessionCrashedInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_RESTORE_SESSION);
}

string16 SessionCrashedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE);
}

int SessionCrashedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 SessionCrashedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
}

bool SessionCrashedInfoBarDelegate::Accept() {
  SessionRestore::RestoreSession(profile_, NULL, true, false,
                                 std::vector<GURL>());
  return true;
}


// Utility functions ----------------------------------------------------------

SessionStartupPref GetSessionStartupPref(const CommandLine& command_line,
                                         Profile* profile) {
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(profile);
  if (command_line.HasSwitch(switches::kRestoreLastSession))
    pref.type = SessionStartupPref::LAST;
  if (command_line.HasSwitch(switches::kIncognito) &&
      pref.type == SessionStartupPref::LAST) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }
  return pref;
}

enum LaunchMode {
  LM_TO_BE_DECIDED = 0,       // Possibly direct launch or via a shortcut.
  LM_AS_WEBAPP,               // Launched as a installed web application.
  LM_WITH_URLS,               // Launched with urls in the cmd line.
  LM_SHORTCUT_NONE,           // Not launched from a shortcut.
  LM_SHORTCUT_NONAME,         // Launched from shortcut but no name available.
  LM_SHORTCUT_UNKNOWN,        // Launched from user-defined shortcut.
  LM_SHORTCUT_QUICKLAUNCH,    // Launched from the quick launch bar.
  LM_SHORTCUT_DESKTOP,        // Launched from a desktop shortcut.
  LM_SHORTCUT_STARTMENU,      // Launched from start menu.
  LM_LINUX_MAC_BEOS           // Other OS buckets start here.
};

#if defined(OS_WIN)
// Undocumented flag in the startup info structure tells us what shortcut was
// used to launch the browser. See http://www.catch22.net/tuts/undoc01 for
// more information. Confirmed to work on XP, Vista and Win7.
LaunchMode GetLaunchShortcutKind() {
  STARTUPINFOW si = { sizeof(si) };
  GetStartupInfoW(&si);
  if (si.dwFlags & 0x800) {
    if (!si.lpTitle)
      return LM_SHORTCUT_NONAME;
    std::wstring shortcut(si.lpTitle);
    // The windows quick launch path is not localized.
    if (shortcut.find(L"\\Quick Launch\\") != std::wstring::npos)
      return LM_SHORTCUT_QUICKLAUNCH;
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string appdata_path;
    env->GetVar("USERPROFILE", &appdata_path);
    if (!appdata_path.empty() &&
        shortcut.find(ASCIIToWide(appdata_path)) != std::wstring::npos)
      return LM_SHORTCUT_DESKTOP;
    return LM_SHORTCUT_UNKNOWN;
  }
  return LM_SHORTCUT_NONE;
}
#else
// TODO(cpu): Port to other platforms.
LaunchMode GetLaunchShortcutKind() {
  return LM_LINUX_MAC_BEOS;
}
#endif

// Log in a histogram the frequency of launching by the different methods. See
// LaunchMode enum for the actual values of the buckets.
void RecordLaunchModeHistogram(LaunchMode mode) {
  int bucket = (mode == LM_TO_BE_DECIDED) ? GetLaunchShortcutKind() : mode;
  UMA_HISTOGRAM_COUNTS_100("Launch.Modes", bucket);
}

static bool in_startup = false;

GURL GetWelcomePageURL() {
  std::string welcome_url = l10n_util::GetStringUTF8(IDS_WELCOME_PAGE_URL);
  return GURL(welcome_url);
}

void UrlsToTabs(const std::vector<GURL>& urls,
                std::vector<BrowserInit::LaunchWithProfile::Tab>* tabs) {
  for (size_t i = 0; i < urls.size(); ++i) {
    BrowserInit::LaunchWithProfile::Tab tab;
    tab.is_pinned = false;
    tab.url = urls[i];
    tabs->push_back(tab);
  }
}

}  // namespace


// BrowserInit ----------------------------------------------------------------

BrowserInit::BrowserInit() {}

BrowserInit::~BrowserInit() {}

void BrowserInit::AddFirstRunTab(const GURL& url) {
  first_run_tabs_.push_back(url);
}

// static
bool BrowserInit::InProcessStartup() {
  return in_startup;
}

bool BrowserInit::LaunchBrowser(const CommandLine& command_line,
                                Profile* profile,
                                const FilePath& cur_dir,
                                bool process_startup,
                                int* return_code) {
  in_startup = process_startup;
  DCHECK(profile);
#if defined(OS_CHROMEOS)
  if (process_startup) {
    // NetworkStateNotifier has to be initialized before Launching browser
    // because the page load can happen in parallel to this UI thread
    // and IO thread may access the NetworkStateNotifier.
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()
        ->AddNetworkManagerObserver(
            chromeos::NetworkStateNotifier::GetInstance());
  }
#endif

  // Continue with the off-the-record profile from here on if --incognito
  if (command_line.HasSwitch(switches::kIncognito))
    profile = profile->GetOffTheRecordProfile();

  BrowserInit::LaunchWithProfile lwp(cur_dir, command_line, this);
  bool launched = lwp.Launch(profile, process_startup);
  in_startup = false;

  if (!launched) {
    LOG(ERROR) << "launch error";
    if (return_code)
      *return_code = ResultCodes::INVALID_CMDLINE_URL;
    return false;
  }

#if defined(OS_CHROMEOS)
  // Create the WmMessageListener so that it can listen for messages regardless
  // of what window has focus.
  chromeos::WmMessageListener::GetInstance();

  // Create the SystemKeyEventListener so it can listen for system keyboard
  // messages regardless of focus.
  chromeos::SystemKeyEventListener::GetInstance();

  // Create the WmOverviewController so it can register with the listener.
  chromeos::WmOverviewController::GetInstance();

  // Install the GView request interceptor that will redirect requests
  // of compatible documents (PDF, etc) to the GView document viewer.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kEnableGView)) {
    chromeos::GViewRequestInterceptor::GetInstance();
  }
  if (process_startup) {
    // TODO(dhg): Try to make this just USBMountObserver::Get()->set_profile
    // and have the constructor take care of everything else.
    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    chromeos::USBMountObserver* observe =
        chromeos::USBMountObserver::GetInstance();
    lib->AddObserver(observe);
    observe->ScanForDevices(lib);
    // Connect the chromeos notifications

    // This observer is a singleton. It is never deleted but the pointer is kept
    // in a static so that it isn't reported as a leak.
    static chromeos::LowBatteryObserver* low_battery_observer =
        new chromeos::LowBatteryObserver(profile);
    chromeos::CrosLibrary::Get()->GetPowerLibrary()->AddObserver(
        low_battery_observer);

    static chromeos::UpdateObserver* update_observer =
        new chromeos::UpdateObserver(profile);
    chromeos::CrosLibrary::Get()->GetUpdateLibrary()->AddObserver(
        update_observer);

    static chromeos::NetworkMessageObserver* network_message_observer =
        new chromeos::NetworkMessageObserver(profile);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()
        ->AddNetworkManagerObserver(network_message_observer);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()
        ->AddCellularDataPlanObserver(network_message_observer);

    static chromeos::BrightnessObserver* brightness_observer =
        new chromeos::BrightnessObserver();
    chromeos::CrosLibrary::Get()->GetBrightnessLibrary()
        ->AddObserver(brightness_observer);

    profile->SetupChromeOSEnterpriseExtensionObserver();
  }
#endif
  return true;
}


// BrowserInit::LaunchWithProfile::Tab ----------------------------------------

BrowserInit::LaunchWithProfile::Tab::Tab() : is_app(false), is_pinned(true) {}

BrowserInit::LaunchWithProfile::Tab::~Tab() {}


// BrowserInit::LaunchWithProfile ---------------------------------------------

BrowserInit::LaunchWithProfile::LaunchWithProfile(
    const FilePath& cur_dir,
    const CommandLine& command_line)
        : cur_dir_(cur_dir),
          command_line_(command_line),
          profile_(NULL),
          browser_init_(NULL) {
}

BrowserInit::LaunchWithProfile::LaunchWithProfile(
    const FilePath& cur_dir,
    const CommandLine& command_line,
    BrowserInit* browser_init)
        : cur_dir_(cur_dir),
          command_line_(command_line),
          profile_(NULL),
          browser_init_(browser_init) {
}

BrowserInit::LaunchWithProfile::~LaunchWithProfile() {
}

bool BrowserInit::LaunchWithProfile::Launch(Profile* profile,
                                            bool process_startup) {
  DCHECK(profile);
  profile_ = profile;

  if (command_line_.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnablePredictorDetailedLog(true);
  if (command_line_.HasSwitch(switches::kDnsPrefetchDisable))
    chrome_browser_net::EnablePredictor(false);

  if (command_line_.HasSwitch(switches::kDumpHistogramsOnExit))
    base::StatisticsRecorder::set_dump_on_exit(true);

  if (command_line_.HasSwitch(switches::kRemoteShellPort)) {
    std::string port_str =
        command_line_.GetSwitchValueASCII(switches::kRemoteShellPort);
    int64 port;
    if (base::StringToInt64(port_str, &port) && port > 0 && port < 65535)
      g_browser_process->InitDebuggerWrapper(static_cast<int>(port), false);
    else
      DLOG(WARNING) << "Invalid remote shell port number " << port;
  } else if (command_line_.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line_.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int64 port;
    if (base::StringToInt64(port_str, &port) && port > 0 && port < 65535)
      g_browser_process->InitDebuggerWrapper(static_cast<int>(port), true);
    else
      DLOG(WARNING) << "Invalid http debugger port number " << port;
  }

  if (command_line_.HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(command_line_.GetSwitchValueASCII(
        switches::kUserAgent));
  }

  // Open the required browser windows and tabs.
  // First, see if we're being run as an application window.
  if (!OpenApplicationWindow(profile)) {
    std::vector<GURL> urls_to_open = GetURLsFromCommandLine(profile_);
    RecordLaunchModeHistogram(urls_to_open.empty()?
                              LM_TO_BE_DECIDED : LM_WITH_URLS);
    ProcessLaunchURLs(process_startup, urls_to_open);

    // If this is an app launch, but we didn't open an app window, it may
    // be an app tab.
    std::string app_id;
    if (IsAppLaunch(NULL, &app_id) && !app_id.empty()) {
      // TODO(erikkay): This could fail if |app_id| is invalid (the app was
      // uninstalled).  We may want to show some reasonable error here.
      Browser::OpenApplication(profile, app_id, NULL);
    }

    if (process_startup) {
      if (browser_defaults::kOSSupportsOtherBrowsers &&
          !command_line_.HasSwitch(switches::kNoDefaultBrowserCheck)) {
        // Check whether we are the default browser.
        CheckDefaultBrowser(profile);
      }
#if defined(OS_MACOSX)
      // Check whether the auto-update system needs to be promoted from user
      // to system.
      KeystoneInfoBar::PromotionInfoBar(profile);
#endif
    }
  } else {
    RecordLaunchModeHistogram(LM_AS_WEBAPP);
  }

#if defined(OS_WIN)
  // Print the selected page if the command line switch exists. Note that the
  // current selected tab would be the page which will be printed.
  if (command_line_.HasSwitch(switches::kPrint)) {
    Browser* browser = BrowserList::GetLastActive();
    browser->Print();
  }
#endif

  // If we're recording or playing back, startup the EventRecorder now
  // unless otherwise specified.
  if (!command_line_.HasSwitch(switches::kNoEvents)) {
    FilePath script_path;
    PathService::Get(chrome::FILE_RECORDED_SCRIPT, &script_path);

    bool record_mode = command_line_.HasSwitch(switches::kRecordMode);
    bool playback_mode = command_line_.HasSwitch(switches::kPlaybackMode);

    if (record_mode && chrome::kRecordModeEnabled)
      base::EventRecorder::current()->StartRecording(script_path);
    if (playback_mode)
      base::EventRecorder::current()->StartPlayback(script_path);
  }

#if defined(OS_WIN)
  if (process_startup)
    ShellIntegration::MigrateChromiumShortcuts();
#endif  // defined(OS_WIN)

  return true;
}

bool BrowserInit::LaunchWithProfile::IsAppLaunch(std::string* app_url,
                                                 std::string* app_id) {
  if (command_line_.HasSwitch(switches::kApp)) {
    if (app_url)
      *app_url = command_line_.GetSwitchValueASCII(switches::kApp);
    return true;
  }
  if (command_line_.HasSwitch(switches::kAppId)) {
    if (app_id)
      *app_id = command_line_.GetSwitchValueASCII(switches::kAppId);
    return true;
  }
  return false;
}

bool BrowserInit::LaunchWithProfile::OpenApplicationWindow(Profile* profile) {
  std::string url_string, app_id;
  if (!IsAppLaunch(&url_string, &app_id))
    return false;

  // This can fail if the app_id is invalid.  It can also fail if the
  // extension is external, and has not yet been installed.
  // TODO(skerner): Do something reasonable here. Pop up a warning panel?
  // Open an URL to the gallery page of the extension id?
  if (!app_id.empty()) {
    ExtensionService* extensions_service = profile->GetExtensionService();
    const Extension* extension =
        extensions_service->GetExtensionById(app_id, false);

    // The extension with id |app_id| may have been uninstalled.
    if (!extension)
      return false;

    // Look at preferences to find the right launch container.  If no
    // preference is set, launch as a window.
    extension_misc::LaunchContainer launch_container =
        extensions_service->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_WINDOW);

    TabContents* app_window = Browser::OpenApplication(
        profile, extension, launch_container, NULL);
    return (app_window != NULL);
  }

  if (url_string.empty())
    return false;

#if defined(OS_WIN)  // Fix up Windows shortcuts.
  ReplaceSubstringsAfterOffset(&url_string, 0, "\\x", "%");
#endif
  GURL url(url_string);

  // Restrict allowed URLs for --app switch.
  if (!url.is_empty() && url.is_valid()) {
    ChildProcessSecurityPolicy *policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(chrome::kFileScheme)) {
      TabContents* app_tab = Browser::OpenAppShortcutWindow(
          profile,
          url,
          true);  // Update app info.
      return (app_tab != NULL);
    }
  }
  return false;
}

void BrowserInit::LaunchWithProfile::ProcessLaunchURLs(
    bool process_startup,
    const std::vector<GURL>& urls_to_open) {
  // If we're starting up in "background mode" (no open browser window) then
  // don't open any browser windows.
  if (process_startup && command_line_.HasSwitch(switches::kNoStartupWindow))
    return;

  if (process_startup && ProcessStartupURLs(urls_to_open)) {
    // ProcessStartupURLs processed the urls, nothing else to do.
    return;
  }

  if (!process_startup &&
      (profile_->GetSessionService() &&
       profile_->GetSessionService()->RestoreIfNecessary(urls_to_open))) {
    // We're already running and session restore wanted to run. This can happen
    // at various points, such as if there is only an app window running and the
    // user double clicked the chrome icon. Return so we don't open the urls.
    return;
  }

  // Session restore didn't occur, open the urls.

  Browser* browser = NULL;
  std::vector<GURL> adjust_urls = urls_to_open;
  if (adjust_urls.empty())
    AddStartupURLs(&adjust_urls);
  else if (!command_line_.HasSwitch(switches::kOpenInNewWindow))
    browser = BrowserList::GetLastActiveWithProfile(profile_);

  OpenURLsInBrowser(browser, process_startup, adjust_urls);
}

bool BrowserInit::LaunchWithProfile::ProcessStartupURLs(
    const std::vector<GURL>& urls_to_open) {
  SessionStartupPref pref = GetSessionStartupPref(command_line_, profile_);
  if (command_line_.HasSwitch(switches::kTestingChannelID) &&
      !command_line_.HasSwitch(switches::kRestoreLastSession) &&
      browser_defaults::kDefaultSessionStartupType !=
      SessionStartupPref::DEFAULT) {
    // When we have non DEFAULT session start type, then we won't open up a
    // fresh session. But none of the tests are written with this in mind, so
    // we explicitly ignore it during testing.
    return false;
  }

  if (pref.type == SessionStartupPref::LAST) {
    if (!profile_->DidLastSessionExitCleanly() &&
        !command_line_.HasSwitch(switches::kRestoreLastSession)) {
      // The last session crashed. It's possible automatically loading the
      // page will trigger another crash, locking the user out of chrome.
      // To avoid this, don't restore on startup but instead show the crashed
      // infobar.
      return false;
    }
    SessionRestore::RestoreSessionSynchronously(profile_, urls_to_open);
    return true;
  }

  std::vector<Tab> tabs = PinnedTabCodec::ReadPinnedTabs(profile_);

  if (!urls_to_open.empty()) {
    // If urls were specified on the command line, use them.
    UrlsToTabs(urls_to_open, &tabs);
  } else if (pref.type == SessionStartupPref::URLS && !pref.urls.empty()) {
    // Only use the set of urls specified in preferences if nothing was
    // specified on the command line.
    UrlsToTabs(pref.urls, &tabs);
  }

  if (tabs.empty())
    return false;

  OpenTabsInBrowser(NULL, true, tabs);
  return true;
}

Browser* BrowserInit::LaunchWithProfile::OpenURLsInBrowser(
    Browser* browser,
    bool process_startup,
    const std::vector<GURL>& urls) {
  std::vector<Tab> tabs;
  UrlsToTabs(urls, &tabs);
  return OpenTabsInBrowser(browser, process_startup, tabs);
}

Browser* BrowserInit::LaunchWithProfile::OpenTabsInBrowser(
        Browser* browser,
        bool process_startup,
        const std::vector<Tab>& tabs) {
  DCHECK(!tabs.empty());
  // If we don't yet have a profile, try to use the one we're given from
  // |browser|. While we may not end up actually using |browser| (since it
  // could be a popup window), we can at least use the profile.
  if (!profile_ && browser)
    profile_ = browser->profile();

  if (!browser || browser->type() != Browser::TYPE_NORMAL) {
    browser = Browser::Create(profile_);
  } else {
#if defined(TOOLKIT_GTK)
    // Setting the time of the last action on the window here allows us to steal
    // focus, which is what the user wants when opening a new tab in an existing
    // browser window.
    gtk_util::SetWMLastUserActionTime(browser->window()->GetNativeHandle());
#endif
  }

#if !defined(OS_MACOSX)
  // In kiosk mode, we want to always be fullscreen, so switch to that now.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    browser->ToggleFullscreenMode();
#endif

  bool first_tab = true;
  for (size_t i = 0; i < tabs.size(); ++i) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    if (!process_startup && !net::URLRequest::IsHandledURL(tabs[i].url))
      continue;

    int add_types = first_tab ? TabStripModel::ADD_SELECTED :
                                TabStripModel::ADD_NONE;
    add_types |= TabStripModel::ADD_FORCE_INDEX;
    if (tabs[i].is_pinned)
      add_types |= TabStripModel::ADD_PINNED;
    int index = browser->GetIndexForInsertionDuringRestore(i);

    browser::NavigateParams params(browser, tabs[i].url,
                                   PageTransition::START_PAGE);
    params.disposition = first_tab ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
    params.tabstrip_index = index;
    params.tabstrip_add_types = add_types;
    params.extension_app_id = tabs[i].app_id;
    browser::Navigate(&params);

    if (profile_ && first_tab && process_startup) {
      AddCrashedInfoBarIfNecessary(params.target_contents->tab_contents());
      AddBadFlagsInfoBarIfNecessary(params.target_contents->tab_contents());
    }

    first_tab = false;
  }
  browser->window()->Show();
  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  browser->GetSelectedTabContents()->view()->SetInitialFocus();

  return browser;
}

void BrowserInit::LaunchWithProfile::AddCrashedInfoBarIfNecessary(
    TabContents* tab) {
  // Assume that if the user is launching incognito they were previously
  // running incognito so that we have nothing to restore from.
  if (!profile_->DidLastSessionExitCleanly() &&
      !profile_->IsOffTheRecord()) {
    // The last session didn't exit cleanly. Show an infobar to the user
    // so that they can restore if they want. The delegate deletes itself when
    // it is closed.
    tab->AddInfoBar(new SessionCrashedInfoBarDelegate(tab));
  }
}

void BrowserInit::LaunchWithProfile::AddBadFlagsInfoBarIfNecessary(
    TabContents* tab) {
  // Unsupported flags for which to display a warning that "stability and
  // security will suffer".
  static const char* kBadFlags[] = {
    // These imply disabling the sandbox.
    switches::kSingleProcess,
    switches::kNoSandbox,
    switches::kInProcessWebGL,
    // These are scary features for developers that shouldn't be turned on
    // persistently.
    switches::kEnableNaCl,
    NULL
  };

  const char* bad_flag = NULL;
  for (const char** flag = kBadFlags; *flag; ++flag) {
    if (command_line_.HasSwitch(*flag)) {
      bad_flag = *flag;
      break;
    }
  }

  if (bad_flag) {
    tab->AddInfoBar(new SimpleAlertInfoBarDelegate(tab, NULL,
        l10n_util::GetStringFUTF16(IDS_BAD_FLAGS_WARNING_MESSAGE,
                                   UTF8ToUTF16(std::string("--") + bad_flag)),
        false));
  }
}

std::vector<GURL> BrowserInit::LaunchWithProfile::GetURLsFromCommandLine(
    Profile* profile) {
  std::vector<GURL> urls;
  const std::vector<CommandLine::StringType>& params = command_line_.args();

  for (size_t i = 0; i < params.size(); ++i) {
    FilePath param = FilePath(params[i]);
    // Handle Vista way of searching - "? <search-term>"
    if (param.value().find(FILE_PATH_LITERAL("? ")) == 0) {
      const TemplateURL* default_provider =
          profile->GetTemplateURLModel()->GetDefaultSearchProvider();
      if (!default_provider || !default_provider->url()) {
        // No search provider available. Just treat this as regular URL.
        urls.push_back(URLFixerUpper::FixupRelativeFile(cur_dir_, param));
        continue;
      }
      const TemplateURLRef* search_url = default_provider->url();
      DCHECK(search_url->SupportsReplacement());
      std::wstring search_term = param.ToWStringHack().substr(2);
      urls.push_back(GURL(search_url->ReplaceSearchTerms(
          *default_provider, WideToUTF16Hack(search_term),
          TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16())));
    } else {
      // This will create a file URL or a regular URL.
      // This call can (in rare circumstances) block the UI thread.
      // Allow it until this bug is fixed.
      //  http://code.google.com/p/chromium/issues/detail?id=60641
      GURL url;
      {
        base::ThreadRestrictions::ScopedAllowIO allow_io;
        url = URLFixerUpper::FixupRelativeFile(cur_dir_, param);
      }
      // Exclude dangerous schemes.
      if (url.is_valid()) {
        ChildProcessSecurityPolicy *policy =
            ChildProcessSecurityPolicy::GetInstance();
        if (policy->IsWebSafeScheme(url.scheme()) ||
            url.SchemeIs(chrome::kFileScheme) ||
#if defined(OS_CHROMEOS)
            // In ChromeOS, allow a settings page to be specified on the
            // command line. See ExistingUserController::OnLoginSuccess.
            (url.spec().find(chrome::kChromeUISettingsURL) == 0) ||
#endif
            (url.spec().compare(chrome::kAboutBlankURL) == 0)) {
          urls.push_back(url);
        }
      }
    }
  }
  return urls;
}

void BrowserInit::LaunchWithProfile::AddStartupURLs(
    std::vector<GURL>* startup_urls) const {
  // If we have urls specified beforehand (i.e. from command line) use them
  // and nothing else.
  if (!startup_urls->empty())
    return;
  // If we have urls specified by the first run master preferences use them
  // and nothing else.
  if (browser_init_) {
    if (!browser_init_->first_run_tabs_.empty()) {
      std::vector<GURL>::iterator it = browser_init_->first_run_tabs_.begin();
      while (it != browser_init_->first_run_tabs_.end()) {
        // Replace magic names for the actual urls.
        if (it->host() == "new_tab_page") {
          startup_urls->push_back(GURL());
        } else if (it->host() == "welcome_page") {
          startup_urls->push_back(GetWelcomePageURL());
        } else {
          startup_urls->push_back(*it);
        }
        ++it;
      }
      browser_init_->first_run_tabs_.clear();
      return;
    }
  }

  // Otherwise open at least the new tab page (and the welcome page, if this
  // is the first time the browser is being started), or the set of URLs
  // specified on the command line.
  startup_urls->push_back(GURL());  // New tab page.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->FindPreference(prefs::kShouldShowWelcomePage) &&
      prefs->GetBoolean(prefs::kShouldShowWelcomePage)) {
    // Reset the preference so we don't show the welcome page next time.
    prefs->ClearPref(prefs::kShouldShowWelcomePage);
    startup_urls->push_back(GetWelcomePageURL());
  }
}

void BrowserInit::LaunchWithProfile::CheckDefaultBrowser(Profile* profile) {
  // We do not check if we are the default browser if:
  // - the user said "don't ask me again" on the infobar earlier.
  // - this is the first launch after the first run flow.
  if (!profile->GetPrefs()->GetBoolean(prefs::kCheckDefaultBrowser) ||
      FirstRun::IsChromeFirstRun()) {
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, new CheckDefaultBrowserTask());
}

bool BrowserInit::ProcessCmdLineImpl(const CommandLine& command_line,
                                     const FilePath& cur_dir,
                                     bool process_startup,
                                     Profile* profile,
                                     int* return_code,
                                     BrowserInit* browser_init) {
  DCHECK(profile);
  if (process_startup) {
    if (command_line.HasSwitch(switches::kDisablePromptOnRepost))
      NavigationController::DisablePromptOnRepost();

    // Look for the testing channel ID ONLY during process startup
    if (command_line.HasSwitch(switches::kTestingChannelID)) {
      std::string testing_channel_id = command_line.GetSwitchValueASCII(
          switches::kTestingChannelID);
      // TODO(sanjeevr) Check if we need to make this a singleton for
      // compatibility with the old testing code
      // If there are any extra parameters, we expect each one to generate a
      // new tab; if there are none then we get one homepage tab.
      int expected_tab_count = 1;
      if (command_line.HasSwitch(switches::kNoStartupWindow)) {
        expected_tab_count = 0;
      } else if (command_line.HasSwitch(switches::kRestoreLastSession)) {
        std::string restore_session_value(
            command_line.GetSwitchValueASCII(switches::kRestoreLastSession));
        base::StringToInt(restore_session_value, &expected_tab_count);
      } else {
        expected_tab_count =
            std::max(1, static_cast<int>(command_line.args().size()));
      }
      if (!CreateAutomationProvider<TestingAutomationProvider>(
          testing_channel_id,
          profile,
          static_cast<size_t>(expected_tab_count)))
        return false;
    }
  }

  bool silent_launch = false;

  if (command_line.HasSwitch(switches::kAutomationClientChannelID)) {
    std::string automation_channel_id = command_line.GetSwitchValueASCII(
        switches::kAutomationClientChannelID);
    // If there are any extra parameters, we expect each one to generate a
    // new tab; if there are none then we have no tabs
    size_t expected_tabs =
        std::max(static_cast<int>(command_line.args().size()), 0);
    if (expected_tabs == 0)
      silent_launch = true;

    if (command_line.HasSwitch(switches::kChromeFrame)) {
      if (!CreateAutomationProvider<ChromeFrameAutomationProvider>(
          automation_channel_id, profile, expected_tabs))
        return false;
    } else {
      if (!CreateAutomationProvider<AutomationProvider>(
          automation_channel_id, profile, expected_tabs))
        return false;
    }
  }

  // If we have been invoked to display a desktop notification on behalf of
  // the service process, we do not want to open any browser windows.
  if (command_line.HasSwitch(switches::kNotifyCloudPrintTokenExpired)) {
    silent_launch = true;
    profile->GetCloudPrintProxyService()->ShowTokenExpiredNotification();
  }
  // If we are just displaying a print dialog we shouldn't open browser
  // windows.
  if (!command_line.GetSwitchValuePath(switches::kCloudPrintFile).empty()) {
    silent_launch = true;
  }


  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

#if defined(OS_CHROMEOS)
  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(switches::kLoginManager) ||
      command_line.HasSwitch(switches::kLoginPassword)) {
    silent_launch = true;
  }
#endif

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
  // Get a list of pointer-devices that should be treated as touch-devices.
  // TODO(sad): Instead of/in addition to getting the list from the
  // command-line, query X for a list of touch devices.
  std::string touch_devices =
    command_line.GetSwitchValueASCII(switches::kTouchDevices);

  if (!touch_devices.empty()) {
    std::vector<std::string> devs;
    std::vector<unsigned int> device_ids;
    unsigned int devid;
    base::SplitString(touch_devices, ',', &devs);
    for (std::vector<std::string>::iterator iter = devs.begin();
        iter != devs.end(); ++iter) {
      if (base::StringToInt(*iter, reinterpret_cast<int*>(&devid))) {
        device_ids.push_back(devid);
      } else {
        DLOG(WARNING) << "Invalid touch-device id: " << *iter;
      }
    }
    views::SetTouchDeviceList(device_ids);
  }
#endif

  // If we don't want to launch a new browser window or tab (in the case
  // of an automation request), we are done here.
  if (!silent_launch) {
    return browser_init->LaunchBrowser(
        command_line, profile, cur_dir, process_startup, return_code);
  }
  return true;
}

template <class AutomationProviderClass>
bool BrowserInit::CreateAutomationProvider(const std::string& channel_id,
                                           Profile* profile,
                                           size_t expected_tabs) {
  scoped_refptr<AutomationProviderClass> automation =
      new AutomationProviderClass(profile);

  if (!automation->InitializeChannel(channel_id))
    return false;
  automation->SetExpectedTabCount(expected_tabs);

  AutomationProviderList* list =
      g_browser_process->InitAutomationProviderList();
  DCHECK(list);
  list->AddProvider(automation);

  return true;
}
