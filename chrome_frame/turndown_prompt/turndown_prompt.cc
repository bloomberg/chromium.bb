// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/turndown_prompt/turndown_prompt.h"

#include <atlbase.h>
#include <shlguid.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome_frame/infobars/infobar_manager.h"
#include "chrome_frame/policy_settings.h"
#include "chrome_frame/ready_mode/internal/ready_mode_web_browser_adapter.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/turndown_prompt/turndown_prompt_content.h"
#include "chrome_frame/utils.h"
#include "grit/chromium_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

void OnUninstallClicked(UrlLauncher* url_launcher);

// Manages the Turndown UI in response to browsing ChromeFrame-rendered
// pages.
class BrowserObserver : public ReadyModeWebBrowserAdapter::Observer {
 public:
  BrowserObserver(IWebBrowser2* web_browser,
                  ReadyModeWebBrowserAdapter* adapter);

  // ReadyModeWebBrowserAdapter::Observer implementation
  virtual void OnNavigateTo(const string16& url);
  virtual void OnRenderInChromeFrame(const string16& url);
  virtual void OnRenderInHost(const string16& url);

 private:
  // Shows the turndown prompt.
  void ShowPrompt();
  void Hide();
  // Returns a self-managed pointer that is not guaranteed to survive handling
  // of Windows events. For safety's sake, retrieve this pointer for each use
  // and do not store it for use outside of scope.
  InfobarManager* GetInfobarManager();

  GURL rendered_url_;
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
  ReadyModeWebBrowserAdapter* adapter_;

  DISALLOW_COPY_AND_ASSIGN(BrowserObserver);
};

// Implements launching of a URL in an instance of IWebBrowser2.
class UrlLauncherImpl : public UrlLauncher {
 public:
  explicit UrlLauncherImpl(IWebBrowser2* web_browser);

  // UrlLauncher implementation
  void LaunchUrl(const string16& url);

 private:
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
};

UrlLauncherImpl::UrlLauncherImpl(IWebBrowser2* web_browser) {
  DCHECK(web_browser);
  web_browser_ = web_browser;
}

void UrlLauncherImpl::LaunchUrl(const string16& url) {
  VARIANT flags = { VT_I4 };
  V_I4(&flags) = navOpenInNewWindow;
  base::win::ScopedBstr location(url.c_str());

  HRESULT hr = web_browser_->Navigate(location, &flags, NULL, NULL, NULL);
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to invoke Navigate on IWebBrowser2. "
                             << "Error: " << hr;
}

BrowserObserver::BrowserObserver(IWebBrowser2* web_browser,
                                 ReadyModeWebBrowserAdapter* adapter)
    : web_browser_(web_browser),
      adapter_(adapter) {
}

void BrowserObserver::OnNavigateTo(const string16& url) {
  if (!net::registry_controlled_domains::SameDomainOrHost(
          GURL(url),
          rendered_url_,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)) {
    rendered_url_ = GURL();
    Hide();
  }
}

void BrowserObserver::OnRenderInChromeFrame(const string16& url) {
  ShowPrompt();
  rendered_url_ = GURL(url);
}

void BrowserObserver::OnRenderInHost(const string16& url) {
  Hide();
  rendered_url_ = GURL(url);
}

void BrowserObserver::ShowPrompt() {
  // This pointer is self-managed and not guaranteed to survive handling of
  // Windows events. For safety's sake, retrieve this pointer for each use and
  // do not store it for use outside of scope.
  InfobarManager* infobar_manager = GetInfobarManager();

  if (infobar_manager) {
    // Owned by infobar_content
    scoped_ptr<UrlLauncher> url_launcher(new UrlLauncherImpl(web_browser_));

    // Owned by infobar_manager
    scoped_ptr<InfobarContent> infobar_content(new TurndownPromptContent(
        url_launcher.release(),
        base::Bind(&OnUninstallClicked,
                   base::Owned(new UrlLauncherImpl(web_browser_)))));

    infobar_manager->Show(infobar_content.release(), TOP_INFOBAR);
  }
}

void BrowserObserver::Hide() {
  InfobarManager* infobar_manager = GetInfobarManager();
  if (infobar_manager)
    infobar_manager->HideAll();
}

InfobarManager* BrowserObserver::GetInfobarManager() {
  HRESULT hr = NOERROR;

  base::win::ScopedComPtr<IOleWindow> ole_window;
  hr = DoQueryService(SID_SShellBrowser, web_browser_, ole_window.Receive());
  if (FAILED(hr) || ole_window == NULL) {
    DLOG(ERROR) << "Failed to query SID_SShellBrowser from IWebBrowser2. "
                << "Error: " << hr;
    return NULL;
  }

  HWND web_browser_hwnd = NULL;
  hr = ole_window->GetWindow(&web_browser_hwnd);
  if (FAILED(hr) || web_browser_hwnd == NULL) {
    DLOG(ERROR) << "Failed to query HWND from IOleWindow. "
                << "Error: " << hr;
    return NULL;
  }

  return InfobarManager::Get(web_browser_hwnd);
}

// Returns true if the module into which this code is linked is installed at
// system-level.
bool IsCurrentModuleSystemLevel() {
  base::FilePath dll_path;
  if (PathService::Get(base::DIR_MODULE, &dll_path))
    return !InstallUtil::IsPerUserInstall(dll_path.value().c_str());
  return false;
}

// Attempts to create a ReadyModeWebBrowserAdapter instance.
bool CreateWebBrowserAdapter(ReadyModeWebBrowserAdapter** adapter) {
  *adapter = NULL;

  CComObject<ReadyModeWebBrowserAdapter>* com_object;
  HRESULT hr =
      CComObject<ReadyModeWebBrowserAdapter>::CreateInstance(&com_object);

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create instance of ReadyModeWebBrowserAdapter. "
                << "Error: " << hr;
    return false;
  }

  com_object->AddRef();
  *adapter = com_object;
  return true;
}

// Attempts to install Turndown prompts in the provided web browser.
bool InstallPrompts(IWebBrowser2* web_browser) {
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> adapter;

  if (!CreateWebBrowserAdapter(adapter.Receive()))
    return false;

  // Pass ownership of our delegate to the BrowserObserver
  scoped_ptr<ReadyModeWebBrowserAdapter::Observer> browser_observer(
      new BrowserObserver(web_browser, adapter));

  // Owns the BrowserObserver
  return adapter->Initialize(web_browser, browser_observer.release());
}

// Launches the Chrome Frame uninstaller in response to user action. This
// implementation should not be used to uninstall MSI-based versions of GCF,
// since those must be removed by way of Windows Installer machinery.
void LaunchChromeFrameUninstaller() {
  BrowserDistribution* dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_FRAME);
  const bool system_level = IsCurrentModuleSystemLevel();
  installer::ProductState product_state;
  if (!product_state.Initialize(system_level, dist)) {
    DLOG(ERROR) << "Chrome frame isn't installed at "
                << (system_level ? "system" : "user") << " level.";
    return;
  }

  CommandLine uninstall_command(product_state.uninstall_command());
  if (uninstall_command.GetProgram().empty()) {
    DLOG(ERROR) << "No uninstall command found in registry.";
    return;
  }

  // Force Uninstall silences the prompt to reboot to complete uninstall.
  uninstall_command.AppendSwitch(installer::switches::kForceUninstall);
  VLOG(1) << "Uninstalling Chrome Frame with command: "
          << uninstall_command.GetCommandLineString();
  base::LaunchProcess(uninstall_command, base::LaunchOptions(), NULL);
}

void LaunchLearnMoreURL(UrlLauncher* url_launcher) {
  url_launcher->LaunchUrl(SimpleResourceLoader::Get(
      IDS_CHROME_FRAME_TURNDOWN_LEARN_MORE_URL));
}

void OnUninstallClicked(UrlLauncher* url_launcher) {
  LaunchChromeFrameUninstaller();
  LaunchLearnMoreURL(url_launcher);
}

}  // namespace

namespace turndown_prompt {

bool IsPromptSuppressed() {
  // See if this is an MSI install of GCF or if updates have been disabled.
  BrowserDistribution* dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_FRAME);

  const bool system_level = IsCurrentModuleSystemLevel();
  bool multi_install = false;

  installer::ProductState product_state;
  if (product_state.Initialize(system_level, dist)) {
    if (product_state.is_msi())
      return true;
    multi_install = product_state.is_multi_install();
  }

  if (multi_install) {
    dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
  }
  if (GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(), NULL) ==
      GoogleUpdateSettings::UPDATES_DISABLED) {
    return true;
  }

  // See if the prompt is explicitly suppressed via GP.
  return PolicySettings::GetInstance()->suppress_turndown_prompt();
}

void Configure(IWebBrowser2* web_browser) {
  if (!IsPromptSuppressed())
    InstallPrompts(web_browser);
}

}  // namespace turndown_prompt
