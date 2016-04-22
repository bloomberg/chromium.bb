// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/public/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/ip_address.h"
#include "ui/gfx/geometry/size.h"

using headless::HeadlessBrowser;
using headless::HeadlessDevToolsClient;
using headless::HeadlessWebContents;
namespace page = headless::page;

namespace {
// Address where to listen to incoming DevTools connections.
const char kDevToolsHttpServerAddress[] = "127.0.0.1";
}

// A sample application which demonstrates the use of the headless API.
class HeadlessShell : public HeadlessWebContents::Observer {
 public:
  HeadlessShell()
      : browser_(nullptr), devtools_client_(HeadlessDevToolsClient::Create()) {}
  ~HeadlessShell() override {}

  void DevToolsTargetReady() override {
    if (!RemoteDebuggingEnabled())
      web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    // TODO(skyostil): Implement more features to demonstrate the devtools API.
  }

  void OnStart(HeadlessBrowser* browser) {
    browser_ = browser;

    base::CommandLine::StringVector args =
        base::CommandLine::ForCurrentProcess()->GetArgs();

    const char kDefaultUrl[] = "about:blank";
    GURL url;
    if (args.empty() || args[0].empty()) {
      url = GURL(kDefaultUrl);
    } else {
      url = GURL(args[0]);
    }
    web_contents_ = browser->CreateWebContents(url, gfx::Size(800, 600));
    if (!web_contents_) {
      LOG(ERROR) << "Navigation failed";
      browser_->Shutdown();
      return;
    }
    web_contents_->AddObserver(this);
  }

  void ShutdownIfNeeded() {
    if (!RemoteDebuggingEnabled()) {
      web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
      web_contents_->RemoveObserver(this);
      web_contents_ = nullptr;
      browser_->Shutdown();
    }
  }

  // HeadlessWebContents::Observer implementation:
  void DocumentOnLoadCompletedInMainFrame() override {
    ShutdownIfNeeded();
  }

  bool RemoteDebuggingEnabled() const {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    return command_line.HasSwitch(switches::kRemoteDebuggingPort);
  }

 private:
  HeadlessBrowser* browser_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  HeadlessWebContents* web_contents_;

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

  if (command_line.HasSwitch(headless::switches::kProxyServer)) {
    std::string proxy_server =
        command_line.GetSwitchValueASCII(headless::switches::kProxyServer);
    net::HostPortPair parsed_proxy_server =
        net::HostPortPair::FromString(proxy_server);
    if (parsed_proxy_server.host().empty() || !parsed_proxy_server.port()) {
      LOG(ERROR) << "Malformed proxy server url";
      return EXIT_FAILURE;
    }
    builder.SetProxyServer(parsed_proxy_server);
  }

  if (command_line.HasSwitch(switches::kHostResolverRules)) {
    builder.SetHostResolverRules(
        command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  }

  return HeadlessBrowserMain(
      builder.Build(),
      base::Bind(&HeadlessShell::OnStart, base::Unretained(&shell)));
}
