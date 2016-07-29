// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_

#include <list>

#include "headless/public/headless_export.h"
#include "net/url_request/url_request_job_factory.h"

namespace headless {
class HeadlessBrowserImpl;

using ProtocolHandlerMap = std::unordered_map<
    std::string,
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>>;

// Represents an isolated session with a unique cache, cookies, and other
// profile/session related data.
class HEADLESS_EXPORT HeadlessBrowserContext {
 public:
  class Builder;

  virtual ~HeadlessBrowserContext() {}

  // TODO(skyostil): Allow saving and restoring contexts (crbug.com/617931).

 protected:
  HeadlessBrowserContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContext);
};

// TODO(alexclarke): We should support this builder for the default context.
class HEADLESS_EXPORT HeadlessBrowserContext::Builder {
 public:
  Builder(Builder&&);
  ~Builder();

  // Set custom network protocol handlers. These can be used to override URL
  // fetching for different network schemes.
  Builder& SetProtocolHandlers(ProtocolHandlerMap protocol_handlers);

  // Specify JS mojo module bindings to be installed, one per mojom file.
  // Note a single mojom file could potentially define many interfaces.
  // |mojom_name| the name including path of the .mojom file.
  // |js_bindings| compiletime generated javascript bindings. Typically loaded
  // from gen/path/name.mojom.js.
  Builder& AddJsMojoBindings(const std::string& mojom_name,
                             const std::string& js_bindings);

  // By default if you add mojo bindings, http and https are disabled because
  // its almost certinly unsafe for arbitary sites on the internet to have
  // access to these bindings.  If you know what you're doing it may be OK to
  // turn them back on. E.g. if headless_lib is being used in a testing
  // framework which serves the web content from disk that's likely ok.
  //
  // That said, best pratice is to add a ProtocolHandler to serve the
  // webcontent over a custom protocol. That way you can be sure that only the
  // things you intend have access to mojo.
  Builder& EnableUnsafeNetworkAccessWithMojoBindings(
      bool enable_http_and_https_if_mojo_used);

  std::unique_ptr<HeadlessBrowserContext> Build();

 private:
  friend class HeadlessBrowserImpl;

  struct MojoBindings {
    MojoBindings();
    MojoBindings(const std::string& mojom_name, const std::string& js_bindings);
    ~MojoBindings();

    std::string mojom_name;
    std::string js_bindings;

   private:
    DISALLOW_COPY_AND_ASSIGN(MojoBindings);
  };

  explicit Builder(HeadlessBrowserImpl* browser);

  HeadlessBrowserImpl* browser_;
  ProtocolHandlerMap protocol_handlers_;
  std::list<MojoBindings> mojo_bindings_;
  bool enable_http_and_https_if_mojo_used_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
