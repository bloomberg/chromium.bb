// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "headless/public/headless_export.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"

namespace base {
class MessagePump;
class SingleThreadTaskRunner;
}

namespace gfx {
class Size;
}

namespace headless {
class HeadlessWebContents;

// This class represents the global headless browser instance. To get a pointer
// to one, call |HeadlessBrowserMain| to initiate the browser main loop. An
// instance of |HeadlessBrowser| will be passed to the callback given to that
// function.
class HEADLESS_EXPORT HeadlessBrowser {
 public:
  struct Options;

  // Create a new browser tab which navigates to |initial_url|. |size| is in
  // physical pixels.
  // We require the user to pass an initial URL to ensure that the renderer
  // gets initialized and eventually becomes ready to be inspected. See
  // HeadlessWebContents::Observer::DevToolsTargetReady.
  virtual HeadlessWebContents* CreateWebContents(const GURL& initial_url,
                                                 const gfx::Size& size) = 0;

  virtual std::vector<HeadlessWebContents*> GetAllWebContents() = 0;

  // Returns a task runner for submitting work to the browser main thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const = 0;

  // Requests browser to stop as soon as possible. |Run| will return as soon as
  // browser stops.
  virtual void Shutdown() = 0;

 protected:
  HeadlessBrowser() {}
  virtual ~HeadlessBrowser() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowser);
};

// Embedding API overrides for the headless browser.
struct HeadlessBrowser::Options {
  Options(const Options& other);
  ~Options();

  class Builder;

  // Command line options to be passed to browser.
  int argc;
  const char** argv;

  std::string user_agent;
  std::string navigator_platform;

  // Address at which DevTools should listen for connections. Disabled by
  // default.
  net::IPEndPoint devtools_endpoint;

  // Address of the HTTP/HTTPS proxy server to use. The system proxy settings
  // are used by default.
  net::HostPortPair proxy_server;

  // Optional message pump that overrides the default. Must outlive the browser.
  base::MessagePump* message_pump;

  // Comma-separated list of rules that control how hostnames are mapped. See
  // chrome::switches::kHostRules for a description for the format.
  std::string host_resolver_rules;

  // Run the browser in single process mode instead of using separate renderer
  // processes as per default. Note that this also disables any sandboxing of
  // web content, which can be a security risk.
  bool single_process_mode;

 private:
  Options(int argc, const char** argv);
};

class HeadlessBrowser::Options::Builder {
 public:
  Builder(int argc, const char** argv);
  Builder();
  ~Builder();

  Builder& SetUserAgent(const std::string& user_agent);
  Builder& EnableDevToolsServer(const net::IPEndPoint& endpoint);
  Builder& SetMessagePump(base::MessagePump* message_pump);
  Builder& SetProxyServer(const net::HostPortPair& proxy_server);
  Builder& SetHostResolverRules(const std::string& host_resolver_rules);
  Builder& SetSingleProcessMode(bool single_process_mode);

  Options Build();

 private:
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

// Main entry point for running the headless browser. This function constructs
// the headless browser instance, passing it to the given
// |on_browser_start_callback| callback. Note that since this function executes
// the main loop, it will only return after HeadlessBrowser::Shutdown() is
// called, returning the exit code for the process.
int HeadlessBrowserMain(
    const HeadlessBrowser::Options& options,
    const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
