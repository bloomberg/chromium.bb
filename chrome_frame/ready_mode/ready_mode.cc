// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/ready_mode.h"

#include <atlbase.h>
#include <shlguid.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "net/base/registry_controlled_domain.h"
#include "chrome_frame/infobars/infobar_manager.h"
#include "chrome_frame/ready_mode/internal/ready_mode_web_browser_adapter.h"
#include "chrome_frame/ready_mode/internal/ready_prompt_content.h"
#include "chrome_frame/ready_mode/internal/registry_ready_mode_state.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/utils.h"

namespace {

// Temporarily disable Ready Mode for 36 hours when the user so indicates.
const int kTemporaryDeclineDurationMinutes = 60 * 36;

class BrowserObserver;

// A helper for BrowserObserver to observe the user's choice in the Ready Mode
// prompt.
class StateObserver : public RegistryReadyModeState::Observer {
 public:
  explicit StateObserver(const base::WeakPtr<BrowserObserver>& ready_mode_ui);
  ~StateObserver();

  // RegistryReadyModeState::Observer implementation
  virtual void OnStateChange(ReadyModeStatus status);

 private:
  base::WeakPtr<BrowserObserver> ready_mode_ui_;

  DISALLOW_COPY_AND_ASSIGN(StateObserver);
};  // class StateObserver

// Manages the Ready Mode UI in response to browsing ChromeFrame- or Host-
// rendered pages. Shows the Ready Mode prompt when the user browses to a GCF-
// enabled page. Hides the prompt when the user begins navigating to a new
// domain or when they navigate to a new page in the same domain that is not
// GCF enabled.
//
// Uses InstallerAdapter and RegistryReadyMode to query and update the
// installation state. Uninstalls the ReadyModeWebBrowserAdapter when the user
// temporarily or permanently exits Ready Mode (decline or accept Chrome Frame).
// If the user declines Chrome Frame, the current page is reloaded in the Host
// renderer.
class BrowserObserver : public ReadyModeWebBrowserAdapter::Observer {
 public:
  BrowserObserver(ready_mode::Delegate* chrome_frame,
                  IWebBrowser2* web_browser,
                  ReadyModeWebBrowserAdapter* adapter);

  // ReadyModeWebBrowserAdapter::Observer implementation
  virtual void OnNavigateTo(const std::wstring& url);
  virtual void OnRenderInChromeFrame(const std::wstring& url);
  virtual void OnRenderInHost(const std::wstring& url);

 private:
  friend class StateObserver;

  // Called by the StateObserver
  void OnReadyModeDisabled();
  void OnReadyModeAccepted();

  // Helpers for showing infobar prompts
  void ShowPrompt();
  void Hide();
  InfobarManager* GetInfobarManager();

  GURL rendered_url_;
  linked_ptr<ready_mode::Delegate> chrome_frame_;
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
  // The adapter owns us, so we use a weak reference
  ReadyModeWebBrowserAdapter* adapter_;
  base::WeakPtrFactory<BrowserObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserObserver);
};  // class BrowserObserver

// Implements launching of a URL in an instance of IWebBrowser2.
class UrlLauncherImpl : public UrlLauncher {
 public:
  explicit UrlLauncherImpl(IWebBrowser2* web_browser);

  // UrlLauncher implementation
  void LaunchUrl(const std::wstring& url);

 private:
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
};  // class UrlLaucherImpl

UrlLauncherImpl::UrlLauncherImpl(IWebBrowser2* web_browser) {
  DCHECK(web_browser);
  web_browser_ = web_browser;
}

void UrlLauncherImpl::LaunchUrl(const std::wstring& url) {
  VARIANT flags = { VT_I4 };
  V_I4(&flags) = navOpenInNewWindow;
  base::win::ScopedBstr location(url.c_str());

  HRESULT hr = web_browser_->Navigate(location, &flags, NULL, NULL, NULL);
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to invoke Navigate on IWebBrowser2. "
                             << "Error: " << hr;
}

StateObserver::StateObserver(
    const base::WeakPtr<BrowserObserver>& ready_mode_ui)
    : ready_mode_ui_(ready_mode_ui) {
}

StateObserver::~StateObserver() {
}

void StateObserver::OnStateChange(ReadyModeStatus status) {
  if (ready_mode_ui_ == NULL)
    return;

  switch (status) {
    case READY_MODE_PERMANENTLY_DECLINED:
    case READY_MODE_TEMPORARILY_DECLINED:
      ready_mode_ui_->OnReadyModeDisabled();
      break;

    case READY_MODE_ACCEPTED:
      ready_mode_ui_->OnReadyModeAccepted();
      break;

    case READY_MODE_ACTIVE:
      break;

    default:
      NOTREACHED();
      break;
  }
}

BrowserObserver::BrowserObserver(ready_mode::Delegate* chrome_frame,
                                 IWebBrowser2* web_browser,
                                 ReadyModeWebBrowserAdapter* adapter)
    : web_browser_(web_browser),
      chrome_frame_(chrome_frame),
      adapter_(adapter),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

void BrowserObserver::OnNavigateTo(const std::wstring& url) {
  if (!net::RegistryControlledDomainService::
          SameDomainOrHost(GURL(url), rendered_url_)) {
    rendered_url_ = GURL();
    Hide();
  }
}

void BrowserObserver::OnRenderInChromeFrame(const std::wstring& url) {
  ShowPrompt();
  rendered_url_ = GURL(url);
}

void BrowserObserver::OnRenderInHost(const std::wstring& url) {
  Hide();
  rendered_url_ = GURL(url);
}

void BrowserObserver::OnReadyModeDisabled() {
  // We don't hold a reference to the adapter, since it owns us (in order to
  // break circular dependency). But we should still AddRef it before
  // invocation.
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> reference(adapter_);

  // adapter_->Uninitialize may delete us, so we should not refer to members
  // after that point.
  base::win::ScopedComPtr<IWebBrowser2> web_browser(web_browser_);

  chrome_frame_->DisableChromeFrame();
  adapter_->Uninitialize();

  VARIANT flags = { VT_I4 };
  V_I4(&flags) = navNoHistory;
  base::win::ScopedBstr location;

  HRESULT hr = web_browser->get_LocationURL(location.Receive());
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to get current location from "
                             << "IWebBrowser2. Error: " << hr;

  if (SUCCEEDED(hr)) {
    hr = web_browser->Navigate(location, &flags, NULL, NULL, NULL);
    DLOG_IF(ERROR, FAILED(hr)) << "Failed to invoke Navigate on IWebBrowser2. "
                               << "Error: " << hr;
  }
}

void BrowserObserver::OnReadyModeAccepted() {
  // See comment in OnReadyModeDisabled.
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> reference(adapter_);
  adapter_->Uninitialize();
}

void BrowserObserver::ShowPrompt() {
  // This pointer is self-managed and not guaranteed to survive handling of
  // Windows events.
  InfobarManager* infobar_manager = GetInfobarManager();

  if (infobar_manager) {
    // Owned by ready_mode_state
    scoped_ptr<RegistryReadyModeState::Observer> ready_mode_state_observer(
        new StateObserver(weak_ptr_factory_.GetWeakPtr()));

    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);

    // Owned by infobar_content
    scoped_ptr<ReadyModeState> ready_mode_state(new RegistryReadyModeState(
        dist->GetStateKey(),
        base::TimeDelta::FromMinutes(kTemporaryDeclineDurationMinutes),
        ready_mode_state_observer.release()));

    // Owned by infobar_content
    scoped_ptr<UrlLauncher> url_launcher(new UrlLauncherImpl(web_browser_));

    // Owned by infobar_manager
    scoped_ptr<InfobarContent> infobar_content(new ReadyPromptContent(
        ready_mode_state.release(), url_launcher.release()));

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

  HWND web_browserhwnd = NULL;
  hr = ole_window->GetWindow(&web_browserhwnd);
  if (FAILED(hr) || web_browserhwnd == NULL) {
    DLOG(ERROR) << "Failed to query HWND from IOleWindow. "
                << "Error: " << hr;
    return NULL;
  }

  return InfobarManager::Get(web_browserhwnd);
}

// Wraps an existing Delegate so that ownership may be shared.
class DelegateWrapper : public ready_mode::Delegate {
 public:
  explicit DelegateWrapper(linked_ptr<ready_mode::Delegate> wrapped);

  // ready_mode::Delegate implementation
  virtual void DisableChromeFrame();

 private:
  linked_ptr<ready_mode::Delegate> wrapped_;

  DISALLOW_COPY_AND_ASSIGN(DelegateWrapper);
};  // class DelegateWrapper

DelegateWrapper::DelegateWrapper(linked_ptr<ready_mode::Delegate> wrapped)
    : wrapped_(wrapped) {
}

void DelegateWrapper::DisableChromeFrame() {
  wrapped_->DisableChromeFrame();
}

// Attempts to create a ReadyModeWebBrowserAdapter instance.
bool CreateWebBrowserAdapter(ReadyModeWebBrowserAdapter** pointer) {
  *pointer = NULL;

  CComObject<ReadyModeWebBrowserAdapter>* com_object;
  HRESULT hr =
      CComObject<ReadyModeWebBrowserAdapter>::CreateInstance(&com_object);

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create instance of ReadyModeWebBrowserAdapter. "
                << "Error: " << hr;
    return false;
  }

  com_object->AddRef();
  *pointer = com_object;
  return true;
}

// Attempts to install Ready Mode prompts in the provided web browser. Will
// notify the provided Delegate if the user declines Chrome Frame temporarily or
// permanently.
bool InstallPrompts(linked_ptr<ready_mode::Delegate> delegate,
                    IWebBrowser2* web_browser) {
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> adapter;

  if (!CreateWebBrowserAdapter(adapter.Receive()))
    return false;

  // Wrap the original delegate so that we can share it with the
  // ReadyModeWebBrowserAdapter
  scoped_ptr<DelegateWrapper> delegate_wrapper(new DelegateWrapper(delegate));

  // Pass ownership of our delegate to the BrowserObserver
  scoped_ptr<ReadyModeWebBrowserAdapter::Observer> browser_observer(
      new BrowserObserver(delegate_wrapper.release(), web_browser, adapter));

  // Owns the BrowserObserver
  return adapter->Initialize(web_browser, browser_observer.release());
}

// Checks if the provided status implies disabling Chrome Frame functionality.
bool ShouldDisableChromeFrame(ReadyModeStatus status) {
  switch (status) {
    case READY_MODE_PERMANENTLY_DECLINED:
    case READY_MODE_TEMPORARILY_DECLINED:
    case READY_MODE_TEMPORARY_DECLINE_EXPIRED:
      return true;

    case READY_MODE_ACCEPTED:
    case READY_MODE_ACTIVE:
      return false;

    default:
      NOTREACHED();
      return true;
  }
}

}  // namespace

namespace ready_mode {

// Determines the current Ready Mode state. If it is active, attempts to set up
// prompting. If we cannot set up prompting, attempts to temporarily disable
// Ready Mode. In the end, if Ready Mode is disabled, pass that information on
// to the Delegate, so that it may disabled Chrome Frame functionality.
void Configure(Delegate* chrome_frame, IWebBrowser2* web_browser) {
  // Take ownership of the delegate
  linked_ptr<Delegate> delegate(chrome_frame);
  chrome_frame = NULL;
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);

  RegistryReadyModeState ready_mode_state(
      dist->GetStateKey(),
      base::TimeDelta::FromMinutes(kTemporaryDeclineDurationMinutes),
      NULL);  // NULL => no observer required

  ReadyModeStatus status = ready_mode_state.GetStatus();

  // If the user temporarily declined Chrome Frame, but the timeout has elapsed,
  // attempt to revert to active Ready Mode state.
  if (status == READY_MODE_TEMPORARY_DECLINE_EXPIRED) {
    ready_mode_state.ExpireTemporaryDecline();
    status = ready_mode_state.GetStatus();
  }

  // If Ready Mode is active, attempt to set up prompting.
  if (status == READY_MODE_ACTIVE) {
    if (!InstallPrompts(delegate, web_browser)) {
      // Failed to set up prompting. Turn off Ready Mode for now.
      ready_mode_state.TemporarilyDeclineChromeFrame();
      status = ready_mode_state.GetStatus();
    }
  }

  // Depending on the state we finally end up in, tell our Delegate to disable
  // Chrome Frame functionality.
  if (ShouldDisableChromeFrame(status))
    delegate->DisableChromeFrame();
}

}  // namespace ready_mode
