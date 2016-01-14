// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "headless/public/headless_export.h"

namespace base {
class SingleThreadTaskRunner;

namespace trace_event {
class TraceConfig;
}
}

namespace gfx {
class Size;
}

namespace net {
class URLRequestContextGetter;
}

namespace headless {
class WebContents;

class HEADLESS_EXPORT HeadlessBrowser {
 public:
  static HeadlessBrowser* Get();

  struct Options;

  // Main routine for running browser.
  // Takes command line args and callback to run as soon as browser starts.
  static int Run(
      const Options& options,
      const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback);

  // Create a new browser tab.
  virtual scoped_ptr<WebContents> CreateWebContents(const gfx::Size& size) = 0;

  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> RendererMainThread() = 0;

  // Requests browser to stop as soon as possible.
  // |Run| will return as soon as browser stops.
  virtual void Stop() = 0;

  virtual void StartTracing(const base::trace_event::TraceConfig& trace_config,
                            const base::Closure& on_tracing_started) = 0;
  virtual void StopTracing(const std::string& log_file_name,
                           const base::Closure& on_tracing_stopped) = 0;

 protected:
  virtual ~HeadlessBrowser() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowser);
};

struct HeadlessBrowser::Options {
  ~Options();

  class Builder;

  // Command line options to be passed to browser.
  int argc;
  const char** argv;

  std::string user_agent;
  std::string navigator_platform;

  static const int kInvalidPort = -1;
  // If not null, create start devtools for remote debugging
  // on specified port.
  int devtools_http_port;

  // Optional URLRequestContextGetter for customizing network stack.
  // Allows overriding:
  // - Cookie storage
  // - HTTP cache
  // - SSL config
  // - Proxy service
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter;

  scoped_ptr<base::MessagePump> message_pump;

 private:
  Options(int argc, const char** argv);
};

class HeadlessBrowser::Options::Builder {
 public:
  Builder(int argc, const char** argv);
  ~Builder();

  Builder& SetUserAgent(const std::string& user_agent);
  Builder& EnableDevToolsServer(int port);
  Builder& SetURLRequestContextGetter(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  Options Build();

 private:
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
