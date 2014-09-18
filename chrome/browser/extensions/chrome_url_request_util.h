// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace extensions {
class Extension;
class InfoMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace chrome_url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                    bool is_incognito,
                                    const Extension* extension,
                                    InfoMap* extension_info_map,
                                    bool* allowed);

// Creates a URLRequestJob for loading component extension resources out of
// a Chrome resource bundle. Returns NULL if the requested resource is not a
// component extension resource.
net::URLRequestJob* MaybeCreateURLRequestResourceBundleJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header);

}  // namespace chrome_url_request_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_
