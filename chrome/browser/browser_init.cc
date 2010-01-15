// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_init.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/event_recorder.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/chrome_frame_automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX)
#include "chrome/browser/cocoa/keystone_infobar.h"
#endif

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/chromeos/mount_library.h"
#include "chrome/browser/chromeos/usb_mount_observer.h"
#include "chrome/browser/views/tabs/tab_overview_message_listener.h"
#endif

namespace {

class SetAsDefaultBrowserTask : public Task {
 public:
  SetAsDefaultBrowserTask() { }
  virtual void Run() {
    ShellIntegration::SetAsDefaultBrowser();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserTask);
};

// The delegate for the infobar shown when Chrome is not the default browser.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit DefaultBrowserInfoBarDelegate(TabContents* contents)
      : ConfirmInfoBarDelegate(contents),
        profile_(contents->profile()),
        action_taken_(false),
        should_expire_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
    // We want the info-bar to stick-around for few seconds and then be hidden
    // on the next navigation after that.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &DefaultBrowserInfoBarDelegate::Expire),
        8000);  // 8 seconds.
  }

  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const {
    return should_expire_;
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    if (!action_taken_)
      UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.Ignored", 1);
    delete this;
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetString(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
  }

  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
       IDR_PRODUCT_ICON_32);
  }

  virtual int GetButtons() const {
    return BUTTON_OK | BUTTON_CANCEL | BUTTON_OK_DEFAULT;
  }

  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    return button == BUTTON_OK ?
        l10n_util::GetString(IDS_SET_AS_DEFAULT_INFOBAR_BUTTON_LABEL) :
        l10n_util::GetString(IDS_DONT_ASK_AGAIN_INFOBAR_BUTTON_LABEL);
  }

  virtual bool Accept() {
    action_taken_ = true;
    UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.SetAsDefault", 1);
    g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
        new SetAsDefaultBrowserTask());
    return true;
  }

  virtual bool Cancel() {
    action_taken_ = true;
    UMA_HISTOGRAM_COUNTS("DefaultBrowserWarning.DontSetAsDefault", 1);
    // User clicked "Don't ask me again", remember that.
    profile_->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, false);
    return true;
  }

  void Expire() {
    should_expire_ = true;
  }

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

class NotifyNotDefaultBrowserTask : public Task {
 public:
  NotifyNotDefaultBrowserTask() { }

  virtual void Run() {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser) {
      // Reached during ui tests.
      return;
    }
    TabContents* tab = browser->GetSelectedTabContents();
    // Don't show the info-bar if there are already info-bars showing.
    // In ChromeBot tests, there might be a race. This line appears to get
    // called during shutdown and |tab| can be NULL.
    if (!tab || tab->infobar_delegate_count() > 0)
      return;
    tab->AddInfoBar(new DefaultBrowserInfoBarDelegate(tab));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifyNotDefaultBrowserTask);
};

class CheckDefaultBrowserTask : public Task {
 public:
  CheckDefaultBrowserTask() {
  }

  virtual void Run() {
    if (ShellIntegration::IsDefaultBrowser())
      return;

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, new NotifyNotDefaultBrowserTask());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CheckDefaultBrowserTask);
};

// A delegate for the InfoBar shown when the previous session has crashed. The
// bar deletes itself automatically after it is closed.
// TODO(timsteele): This delegate can leak when a tab is closed, see
// http://crbug.com/6520
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit SessionCrashedInfoBarDelegate(TabContents* contents)
      : ConfirmInfoBarDelegate(contents),
        profile_(contents->profile()) {
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    delete this;
  }
  virtual std::wstring GetMessageText() const {
    return l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_MESSAGE);
  }
  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_RESTORE_SESSION);
  }
  virtual int GetButtons() const { return BUTTON_OK; }
  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    return l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
  }
  virtual bool Accept() {
    // Restore the session.
    SessionRestore::RestoreSession(profile_, NULL, true, false,
                                   std::vector<GURL>());
    return true;
  }

 private:
  // The Profile that we restore sessions from.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

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
    std::wstring appdata_path = base::SysInfo::GetEnvVar(L"USERPROFILE");
    if (!appdata_path.empty() &&
        shortcut.find(appdata_path) != std::wstring::npos)
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

bool LaunchBrowser(const CommandLine& command_line, Profile* profile,
                   const std::wstring& cur_dir, bool process_startup,
                   int* return_code, BrowserInit* browser_init) {
  in_startup = process_startup;
  DCHECK(profile);

  // Continue with the off-the-record profile from here on if --incognito
  if (command_line.HasSwitch(switches::kIncognito))
    profile = profile->GetOffTheRecordProfile();

  BrowserInit::LaunchWithProfile lwp(cur_dir, command_line, browser_init);
  bool launched = lwp.Launch(profile, process_startup);
  in_startup = false;

  if (!launched) {
    LOG(ERROR) << "launch error";
    if (return_code)
      *return_code = ResultCodes::INVALID_CMDLINE_URL;
    return false;
  }

#if defined(OS_CHROMEOS)
  // Create the TabOverviewMessageListener so that it can listen for messages
  // regardless of what window has focus.
  TabOverviewMessageListener::instance();

  // Install the GView request interceptor that will redirect requests
  // of compatible documents (PDF, etc) to the GView document viewer.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kEnableGView)) {
    chromeos::GViewRequestInterceptor::GetGViewRequestInterceptor();
  }
  if (process_startup) {
    // TODO(dhg): Try to make this just USBMountObserver::Get()->set_profile
    // and have the constructor take care of everything else.
    chromeos::MountLibrary* lib = chromeos::MountLibrary::Get();
    chromeos::USBMountObserver* observe = chromeos::USBMountObserver::Get();
    observe->set_profile(profile);
    lib->AddObserver(observe);
  }
#endif
  return true;
}

GURL GetWelcomePageURL() {
  std::string welcome_url = l10n_util::GetStringUTF8(IDS_WELCOME_PAGE_URL);
  return GURL(welcome_url);
}

void ShowPackExtensionMessage(const std::wstring caption,
    const std::wstring message) {
#if defined(OS_WIN)
  win_util::MessageBox(NULL, message, caption, MB_OK | MB_SETFOREGROUND);
#else
  // Just send caption & text to stdout on mac & linux.
  std::string out_text = WideToASCII(caption);
  out_text.append("\n\n");
  out_text.append(WideToASCII(message));
  out_text.append("\n");
  printf("%s", out_text.c_str());
#endif
}

}  // namespace

// static
bool BrowserInit::InProcessStartup() {
  return in_startup;
}

// LaunchWithProfile ----------------------------------------------------------

BrowserInit::LaunchWithProfile::LaunchWithProfile(
    const std::wstring& cur_dir,
    const CommandLine& command_line)
        : cur_dir_(cur_dir),
          command_line_(command_line),
          profile_(NULL),
          browser_init_(NULL) {
}

BrowserInit::LaunchWithProfile::LaunchWithProfile(
    const std::wstring& cur_dir,
    const CommandLine& command_line,
    BrowserInit* browser_init)
        : cur_dir_(cur_dir),
          command_line_(command_line),
          profile_(NULL),
          browser_init_(browser_init) {
}

bool BrowserInit::LaunchWithProfile::Launch(Profile* profile,
                                            bool process_startup) {
  DCHECK(profile);
  profile_ = profile;

  if (command_line_.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnableDnsDetailedLog(true);
  if (command_line_.HasSwitch(switches::kDnsPrefetchDisable))
    chrome_browser_net::EnableDnsPrefetch(false);

  if (command_line_.HasSwitch(switches::kDumpHistogramsOnExit))
    StatisticsRecorder::set_dump_on_exit(true);

  if (command_line_.HasSwitch(switches::kRemoteShellPort)) {
    if (!RenderProcessHost::run_renderer_in_process()) {
      std::string port_str =
          command_line_.GetSwitchValueASCII(switches::kRemoteShellPort);
      int64 port = StringToInt64(port_str);
      if (port > 0 && port < 65535) {
        g_browser_process->InitDebuggerWrapper(static_cast<int>(port));
      } else {
        DLOG(WARNING) << "Invalid port number " << port;
      }
    }
  }

  if (command_line_.HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(command_line_.GetSwitchValueASCII(
        switches::kUserAgent));
  }

  // Open the required browser windows and tabs.
  // First, see if we're being run as a web application (thin frame window).
  if (!OpenApplicationURL(profile)) {
    std::vector<GURL> urls_to_open = GetURLsFromCommandLine(profile_);
    RecordLaunchModeHistogram(urls_to_open.empty()?
                              LM_TO_BE_DECIDED : LM_WITH_URLS);
    // Always attempt to restore the last session. OpenStartupURLs only opens
    // the home pages if no additional URLs were passed on the command line.
    if (!OpenStartupURLs(process_startup, urls_to_open)) {
      // Add the home page and any special first run URLs.
      Browser* browser = NULL;
      if (urls_to_open.empty())
        AddStartupURLs(&urls_to_open);
      else if (!command_line_.HasSwitch(switches::kOpenInNewWindow))
        browser = BrowserList::GetLastActive();

      OpenURLsInBrowser(browser, process_startup, urls_to_open);
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

bool BrowserInit::LaunchWithProfile::OpenApplicationURL(Profile* profile) {
  if (!command_line_.HasSwitch(switches::kApp))
    return false;

  std::string url_string(command_line_.GetSwitchValueASCII(switches::kApp));
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
      Browser::OpenApplicationWindow(profile, url);
      return true;
    }
  }
  return false;
}

bool BrowserInit::LaunchWithProfile::OpenStartupURLs(
    bool is_process_startup,
    const std::vector<GURL>& urls_to_open) {
  SessionStartupPref pref = GetSessionStartupPref(command_line_, profile_);
  if (is_process_startup &&
      command_line_.HasSwitch(switches::kTestingChannelID) &&
      !command_line_.HasSwitch(switches::kRestoreLastSession) &&
      browser_defaults::kDefaultSessionStartupType !=
      SessionStartupPref::DEFAULT) {
    // When we have non DEFAULT session start type, then we won't open up a
    // fresh session. But none of the tests are written with this in mind, so
    // we explicitly ignore it during testing.
    return false;
  }
  switch (pref.type) {
    case SessionStartupPref::LAST:
      if (!is_process_startup)
        return false;

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

    case SessionStartupPref::URLS:
      // When the user launches the app only open the default set of URLs if
      // we aren't going to open any URLs on the command line.
      if (urls_to_open.empty()) {
        if (pref.urls.empty()) {
          // Open a New Tab page.
          std::vector<GURL> urls;
          urls.push_back(GURL(chrome::kChromeUINewTabURL));
          OpenURLsInBrowser(NULL, is_process_startup, urls);
          return true;
        }
        OpenURLsInBrowser(NULL, is_process_startup, pref.urls);
        return true;
      }
      return false;

    default:
      return false;
  }
}

Browser* BrowserInit::LaunchWithProfile::OpenURLsInBrowser(
    Browser* browser,
    bool process_startup,
    const std::vector<GURL>& urls) {
  DCHECK(!urls.empty());
  // If we don't yet have a profile, try to use the one we're given from
  // |browser|. While we may not end up actually using |browser| (since it
  // could be a popup window), we can at least use the profile.
  if (!profile_ && browser)
    profile_ = browser->profile();

  int pin_count = 0;
  if (!browser) {
    std::string pin_count_string =
        command_line_.GetSwitchValueASCII(switches::kPinnedTabCount);
    if (!pin_count_string.empty())
      pin_count = StringToInt(pin_count_string);
  }
  if (!browser || browser->type() != Browser::TYPE_NORMAL)
    browser = Browser::Create(profile_);

#if !defined(OS_MACOSX)
  // In kiosk mode, we want to always be fullscreen, so switch to that now.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    browser->ToggleFullscreenMode();
#endif

  for (size_t i = 0; i < urls.size(); ++i) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    if (!process_startup && !URLRequest::IsHandledURL(urls[i]))
      continue;
    TabContents* tab = browser->AddTabWithURL(
        urls[i], GURL(), PageTransition::START_PAGE, (i == 0), -1, false, NULL);
    if (i < static_cast<size_t>(pin_count))
      browser->tabstrip_model()->SetTabPinned(browser->tab_count() - 1, true);
    if (profile_ && i == 0 && process_startup)
      AddCrashedInfoBarIfNecessary(tab);
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

std::vector<GURL> BrowserInit::LaunchWithProfile::GetURLsFromCommandLine(
    Profile* profile) {
  std::vector<GURL> urls;
  std::vector<std::wstring> params = command_line_.GetLooseValues();

  for (size_t i = 0; i < params.size(); ++i) {
    std::wstring& value = params[i];
    // Handle Vista way of searching - "? <search-term>"
    if (value.find(L"? ") == 0) {
      const TemplateURL* default_provider =
          profile->GetTemplateURLModel()->GetDefaultSearchProvider();
      if (!default_provider || !default_provider->url()) {
        // No search provider available. Just treat this as regular URL.
        urls.push_back(
            GURL(WideToUTF8(URLFixerUpper::FixupRelativeFile(cur_dir_,
                                                             value))));
        continue;
      }
      const TemplateURLRef* search_url = default_provider->url();
      DCHECK(search_url->SupportsReplacement());
      urls.push_back(GURL(WideToUTF8(search_url->ReplaceSearchTerms(
          *default_provider, value.substr(2),
          TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()))));
    } else {
      // This will create a file URL or a regular URL.
      GURL url = GURL(WideToUTF8(
          URLFixerUpper::FixupRelativeFile(cur_dir_, value)));
      // Exclude dangerous schemes.
      if (url.is_valid()) {
        ChildProcessSecurityPolicy *policy =
            ChildProcessSecurityPolicy::GetInstance();
        if (policy->IsWebSafeScheme(url.scheme()) ||
            url.SchemeIs(chrome::kFileScheme) ||
            !url.spec().compare(chrome::kAboutBlankURL)) {
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
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE, new CheckDefaultBrowserTask());
}

bool BrowserInit::ProcessCmdLineImpl(const CommandLine& command_line,
                                     const std::wstring& cur_dir,
                                     bool process_startup,
                                     Profile* profile,
                                     int* return_code,
                                     BrowserInit* browser_init) {
  DCHECK(profile);
  if (process_startup) {
    const std::string popup_count_string =
        command_line.GetSwitchValueASCII(switches::kOmniBoxPopupCount);
    if (!popup_count_string.empty()) {
      int count = 0;
      if (StringToInt(popup_count_string, &count)) {
        const int popup_count = std::max(0, count);
        AutocompleteResult::set_max_matches(popup_count);
        AutocompleteProvider::set_max_matches(popup_count / 2);
      }
    }

    if (command_line.HasSwitch(switches::kDisablePromptOnRepost))
      NavigationController::DisablePromptOnRepost();

    const std::string tab_count_string = command_line.GetSwitchValueASCII(
        switches::kTabCountToLoadOnSessionRestore);
    if (!tab_count_string.empty()) {
      int count = 0;
      if (StringToInt(tab_count_string, &count)) {
        const int tab_count = std::max(0, count);
        SessionRestore::num_tabs_to_load_ = static_cast<size_t>(tab_count);
      }
    }

    // Look for the testing channel ID ONLY during process startup
    if (command_line.HasSwitch(switches::kTestingChannelID)) {
      std::string testing_channel_id = command_line.GetSwitchValueASCII(
          switches::kTestingChannelID);
      // TODO(sanjeevr) Check if we need to make this a singleton for
      // compatibility with the old testing code
      // If there are any loose parameters, we expect each one to generate a
      // new tab; if there are none then we get one homepage tab.
      int expected_tab_count = 1;
      if (command_line.HasSwitch(switches::kRestoreLastSession)) {
        std::string restore_session_value(
            command_line.GetSwitchValueASCII(switches::kRestoreLastSession));
        StringToInt(restore_session_value, &expected_tab_count);
      } else {
        expected_tab_count =
            std::max(1, static_cast<int>(command_line.GetLooseValues().size()));
      }
      CreateAutomationProvider<TestingAutomationProvider>(
          testing_channel_id,
          profile,
          static_cast<size_t>(expected_tab_count));
    }

    if (command_line.HasSwitch(switches::kPackExtension)) {
      // Input Paths.
      FilePath src_dir = command_line.GetSwitchValuePath(
          switches::kPackExtension);
      FilePath private_key_path;
      if (command_line.HasSwitch(switches::kPackExtensionKey)) {
        private_key_path = command_line.GetSwitchValuePath(
            switches::kPackExtensionKey);
      }

      // Output Paths.
      FilePath output(src_dir.DirName().Append(src_dir.BaseName().value()));
      FilePath crx_path(output);
      crx_path = crx_path.ReplaceExtension(chrome::kExtensionFileExtension);
      FilePath output_private_key_path;
      if (private_key_path.empty()) {
        output_private_key_path = FilePath(output);
        output_private_key_path =
            output_private_key_path.ReplaceExtension(FILE_PATH_LITERAL("pem"));
      }

      // TODO(port): Creation & running is removed from mac & linux because
      // ExtensionCreator depends on base/crypto/rsa_private_key and
      // base/crypto/signature_creator, both of which only have windows
      // implementations.
      scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
      if (creator->Run(src_dir, crx_path, private_key_path,
          output_private_key_path)) {
        std::wstring message;
        if (private_key_path.value().empty()) {
          message = StringPrintf(
              L"Created the following files:\n\n"
              L"Extension: %ls\n"
              L"Key File: %ls\n\n"
              L"Keep your key file in a safe place. You will need it to create "
              L"new versions of your extension.",
              crx_path.ToWStringHack().c_str(),
              output_private_key_path.ToWStringHack().c_str());
        } else {
          message = StringPrintf(L"Created the extension:\n\n%ls",
                                 crx_path.ToWStringHack().c_str());
        }
        ShowPackExtensionMessage(L"Extension Packaging Success", message);
      } else {
        ShowPackExtensionMessage(L"Extension Packaging Error",
            UTF8ToWide(creator->error_message()));
        return false;
      }
      return false;
    }
  }

  bool silent_launch = false;
  if (command_line.HasSwitch(switches::kAutomationClientChannelID)) {
    std::string automation_channel_id = command_line.GetSwitchValueASCII(
        switches::kAutomationClientChannelID);
    // If there are any loose parameters, we expect each one to generate a
    // new tab; if there are none then we have no tabs
    size_t expected_tabs =
        std::max(static_cast<int>(command_line.GetLooseValues().size()),
                 0);
    if (expected_tabs == 0)
      silent_launch = true;

    if (command_line.HasSwitch(switches::kChromeFrame)) {
      CreateAutomationProvider<ChromeFrameAutomationProvider>(
          automation_channel_id, profile, expected_tabs);
    } else {
      CreateAutomationProvider<AutomationProvider>(automation_channel_id,
                                                   profile, expected_tabs);
    }
  }

  if (command_line.HasSwitch(switches::kUseFlip)) {
    std::string flip_mode =
        command_line.GetSwitchValueASCII(switches::kUseFlip);
    net::HttpNetworkLayer::EnableFlip(flip_mode);
  }

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::wstring allowed_ports =
      command_line.GetSwitchValue(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

  if (command_line.HasSwitch(switches::kEnableUserDataDirProfiles)) {
    // Update user data dir profiles when kEnableUserDataDirProfiles is enabled.
    // Profile enumeration would be scheduled on file thread and when
    // enumeration is done, the profile list in BrowserProcess would be
    // updated on ui thread.
    UserDataManager::Get()->RefreshUserDataDirProfiles();
  }

  // If we don't want to launch a new browser window or tab (in the case
  // of an automation request), we are done here.
  if (!silent_launch) {
    return LaunchBrowser(command_line, profile, cur_dir, process_startup,
                         return_code, browser_init);
  }
  return true;
}

template <class AutomationProviderClass>
void BrowserInit::CreateAutomationProvider(const std::string& channel_id,
                                           Profile* profile,
                                           size_t expected_tabs) {
  scoped_refptr<AutomationProviderClass> automation =
      new AutomationProviderClass(profile);
  automation->ConnectToChannel(channel_id);
  automation->SetExpectedTabCount(expected_tabs);

  AutomationProviderList* list =
      g_browser_process->InitAutomationProviderList();
  DCHECK(list);
  list->AddProvider(automation);
}
