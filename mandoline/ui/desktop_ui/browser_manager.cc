// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/desktop_ui/browser_manager.h"

#include <utility>

#include "base/command_line.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "mandoline/ui/desktop_ui/browser_window.h"
#include "mojo/shell/public/cpp/shell.h"

namespace mandoline {

namespace {

const char kGoogleURL[] = "http://www.google.com";

}  // namespace

BrowserManager::BrowserManager()
    : shell_(nullptr), startup_ticks_(base::TimeTicks::Now()) {}

BrowserManager::~BrowserManager() {
  while (!browsers_.empty())
    (*browsers_.begin())->Close();
  DCHECK(browsers_.empty());
}

BrowserWindow* BrowserManager::CreateBrowser(const GURL& default_url) {
  BrowserWindow* browser = new BrowserWindow(shell_, host_factory_.get(), this);
  browsers_.insert(browser);
  browser->LoadURL(default_url);
  return browser;
}

void BrowserManager::BrowserWindowClosed(BrowserWindow* browser) {
  DCHECK_GT(browsers_.count(browser), 0u);
  browsers_.erase(browser);
  if (browsers_.empty())
    shell_->Quit();
}

void BrowserManager::LaunchURL(const mojo::String& url) {
  DCHECK(!browsers_.empty());
  // TODO(fsamuel): Create a new Browser once we support multiple browser
  // windows.
  (*browsers_.begin())->LoadURL(GURL(url.get()));
}

void BrowserManager::Initialize(mojo::Shell* shell, const std::string& url,
                                uint32_t id) {
  shell_ = shell;
  tracing_.Initialize(shell, url);

  shell_->ConnectToService("mojo:mus", &host_factory_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Create a Browser for each valid URL in the command line.
  for (const auto& arg : command_line->GetArgs()) {
    GURL url(arg);
    if (url.is_valid())
      CreateBrowser(url);
  }
  // If there were no valid URLs in the command line create a Browser with the
  // default URL.
  if (browsers_.empty())
    CreateBrowser(GURL(kGoogleURL));
}

bool BrowserManager::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<LaunchHandler>(this);
  return true;
}

void BrowserManager::Create(mojo::ApplicationConnection* connection,
                            mojo::InterfaceRequest<LaunchHandler> request) {
  launch_handler_bindings_.AddBinding(this, std::move(request));
}

}  // namespace mandoline
