// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/ip_address.h"
#include "ui/gfx/geometry/size.h"

using headless::HeadlessBrowser;
using headless::HeadlessWebContents;

namespace {
// Address where to listen to incoming DevTools connections.
const char kDevToolsHttpServerAddress[] = "127.0.0.1";
}

// A sample application which demonstrates the use of the headless API.
class HeadlessShell : public HeadlessWebContents::Observer {
 public:
  HeadlessShell() : browser_(nullptr) {}
  ~HeadlessShell() override {
    if (web_contents_)
      web_contents_->RemoveObserver(this);
  }

  void OnStart(HeadlessBrowser* browser) {
    browser_ = browser;
    web_contents_ = browser->CreateWebContents(gfx::Size(800, 600));
    web_contents_->AddObserver(this);

    base::CommandLine::StringVector args =
        base::CommandLine::ForCurrentProcess()->GetArgs();

    const char kDefaultUrl[] = "about:blank";
    GURL url;
    if (args.empty() || args[0].empty()) {
      url = GURL(kDefaultUrl);
    } else {
      url = GURL(args[0]);
    }
    if (!web_contents_->OpenURL(url)) {
      LOG(ERROR) << "Navigation failed";
      web_contents_ = nullptr;
      browser_->Shutdown();
    }
  }

  void ShutdownIfNeeded() {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
      web_contents_ = nullptr;
      browser_->Shutdown();
    }
  }

  // HeadlessWebContents::Observer implementation:
  void DocumentOnLoadCompletedInMainFrame() override {
    ShutdownIfNeeded();
  }

 private:
  HeadlessBrowser* browser_;  // Not owned.
  scoped_ptr<HeadlessWebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessShell);
};

int main(int argc, const char** argv) {
  HeadlessShell shell;
  HeadlessBrowser::Options::Builder builder(argc, argv);

  // Enable devtools if requested.
  base::CommandLine command_line(argc, argv);
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int parsed_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &parsed_port) &&
        base::IsValueInRangeForNumericType<uint16_t>(parsed_port)) {
      net::IPAddress devtools_address;
      bool result =
          devtools_address.AssignFromIPLiteral(kDevToolsHttpServerAddress);
      DCHECK(result);
      builder.EnableDevToolsServer(net::IPEndPoint(
          devtools_address, base::checked_cast<uint16_t>(parsed_port)));
    }
  }

  return HeadlessBrowserMain(
      builder.Build(),
      base::Bind(&HeadlessShell::OnStart, base::Unretained(&shell)));
}
