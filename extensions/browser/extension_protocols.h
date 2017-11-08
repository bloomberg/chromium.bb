// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_
#define EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
class FilePath;
class Time;
}

namespace content {
class RenderFrameHost;
}

namespace net {
class URLRequest;
class URLRequestJob;
class HttpResponseHeaders;
}

class GURL;

namespace extensions {
class InfoMap;

using ExtensionProtocolTestHandler =
    base::Callback<net::URLRequestJob*(net::URLRequest*,
                                       net::NetworkDelegate*,
                                       const base::FilePath&)>;

// Builds HTTP headers for an extension request. Hashes the time to avoid
// exposing the exact user installation time of the extension.
scoped_refptr<net::HttpResponseHeaders> BuildHttpHeaders(
    const std::string& content_security_policy,
    bool send_cors_header,
    const base::Time& last_modified_time);

// Creates the handlers for the chrome-extension:// scheme. Pass true for
// |is_incognito| only for incognito profiles and not for Chrome OS guest mode
// profiles.
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateExtensionProtocolHandler(bool is_incognito, InfoMap* extension_info_map);

// Allows tests to set a special handler for chrome-extension:// urls. Note
// that this goes through all the normal security checks; it's essentially a
// way to map extra resources to be included in extensions.
void SetExtensionProtocolTestHandler(ExtensionProtocolTestHandler* handler);

// Creates a new content::mojom::URLLoaderFactory implementation suitable for
// handling navigation requests to extension URLs.
std::unique_ptr<content::mojom::URLLoaderFactory>
CreateExtensionNavigationURLLoaderFactory(content::RenderFrameHost* frame_host);

// Attempts to create a content::mojom::URLLoaderFactory implementation suitable
// for handling subresource requests for extension URLs from |frame_host|. May
// return null if |frame_host| is never allowed to load extension subresources
// from its current navigation URL.
std::unique_ptr<content::mojom::URLLoaderFactory>
MaybeCreateExtensionSubresourceURLLoaderFactory(
    content::RenderFrameHost* frame_host,
    const GURL& frame_url);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PROTOCOLS_H_
