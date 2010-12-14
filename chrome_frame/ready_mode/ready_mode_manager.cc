// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/ready_mode_manager.h"

#include <exdisp.h>
#include <atlbase.h>
#include <shlguid.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "net/base/registry_controlled_domain.h"
#include "chrome_frame/infobars/infobar_manager.h"
#include "chrome_frame/ready_mode/internal/ready_prompt_content.h"
#include "chrome_frame/ready_mode/internal/registry_ready_mode_state.h"
#include "chrome_frame/utils.h"

namespace {

const int kTemporaryDeclineDurationMinutes = 1;

};  // namespace

class ReadyModeManagerImpl : public ReadyModeManager {
 public:
  bool Initialize(IUnknown* site) {
    DCHECK(!web_browser_);
    DCHECK(site);

    if (web_browser_ != NULL)
      return false;

    if (site != NULL)
      web_browser_.QueryFrom(site);

    return web_browser_ != NULL;
  }

 protected:
  virtual void OnDeclineChromeFrame() {
    VARIANT flags = { VT_I4 };
    V_I4(&flags) = navNoHistory;
    web_browser_->Navigate(CComBSTR(ASCIIToWide(rendered_url_.spec()).c_str()),
                           &flags, NULL, NULL, NULL);
  }

  virtual InfobarManager* GetInfobarManager() {
    base::win::ScopedComPtr<IOleWindow> ole_window;
    HWND web_browserhwnd = NULL;

    if (web_browser_ != NULL)
      DoQueryService(SID_SShellBrowser, web_browser_, ole_window.Receive());

    if (ole_window != NULL)
      ole_window->GetWindow(&web_browserhwnd);

    if (web_browserhwnd != NULL)
      return InfobarManager::Get(web_browserhwnd);

    return NULL;
  }

 private:
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
};

class DisableReadyModeObserver : public RegistryReadyModeState::Observer {
 public:
  DisableReadyModeObserver(IWebBrowser2* web_browser)
      : web_browser_(web_browser) {
    DCHECK(web_browser != NULL);
  }
  virtual void OnStateChange() {
    VARIANT flags = { VT_I4 };
    V_I4(&flags) = navNoHistory;
    web_browser_->Navigate(CComBSTR(ASCIIToWide(rendered_url_.spec()).c_str()),
                           &flags, NULL, NULL, NULL);
  }
 private:
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
};

class ReadyModeUIImpl : public ReadyModeWebBrowserAdapter::ReadyModeUI {
 public:
  virtual void ShowPrompt() {
    InfobarManager* infobar_manager = GetInfobarManager();
    if (infobar_manager) {
      InstallationState* = new DummyInstallationState();

      ReadyModeState* ready_mode_state = new RegistryReadyModeState(
        kChromeFrameConfigKey,
        base::TimeDelta::FromMinutes(kTemporaryDeclineDurationMinutes),
        installation_state,
        observer);
      base::WeakPtr<ReadyModeManager> weak_ptr(weak_ptr_factory_.GetWeakPtr());
      infobar_manager->Show(new ReadyPromptContent(NULL /* TODO(erikwright)*/), TOP_INFOBAR);
    }
  }
  virtual void Hide() {
  }

 private:
  virtual InfobarManager* GetInfobarManager() {
    base::win::ScopedComPtr<IOleWindow> ole_window;
    HWND web_browserhwnd = NULL;

    if (web_browser_ != NULL)
      DoQueryService(SID_SShellBrowser, web_browser_, ole_window.Receive());

    if (ole_window != NULL)
      ole_window->GetWindow(&web_browserhwnd);

    if (web_browserhwnd != NULL)
      return InfobarManager::Get(web_browserhwnd);

    return NULL;
  }

  base::win::ScopedComPtr<IWebBrowser2> web_browser_;
};

void ReadyMode::Configure(ChromeFrameIntegration* integration,
                          IWebBrowser2* site) {
  
}

ReadyModeManager::ReadyModeManager()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

ReadyModeManager::~ReadyModeManager() {
}

ReadyModeManager* ReadyModeManager::CreateReadyModeManager(IUnknown* site) {
  scoped_ptr<ReadyModeManagerImpl> impl(new ReadyModeManagerImpl());

  if (impl->Initialize(site))
    return impl.release();

  return NULL;
}

void ReadyModeManager::BeginNavigationTo(std::string http_method, std::wstring url) {
  if (!net::RegistryControlledDomainService::SameDomainOrHost(GURL(url),
                                                              rendered_url_)) {
    InfobarManager* infobar_manager = GetInfobarManager();
    if (infobar_manager)
      infobar_manager->HideAll();

    rendered_url_ = GURL();
  }
}

void ReadyModeManager::RenderingInHost(std::string http_method, std::wstring url) {
  InfobarManager* infobar_manager = GetInfobarManager();
  if (infobar_manager)
    infobar_manager->HideAll();
  rendered_url_ = GURL(url);
}

void ReadyModeManager::RenderingInChromeFrame(std::string http_method, std::wstring url) {
  InfobarManager* infobar_manager = GetInfobarManager();
  if (infobar_manager) {
    base::WeakPtr<ReadyModeManager> weak_ptr(weak_ptr_factory_.GetWeakPtr());
    infobar_manager->Show(new ReadyPromptContent(NULL /* TODO(erikwright)*/), TOP_INFOBAR);
  }
  rendered_url_ = GURL(url);
}
