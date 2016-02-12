// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/switches.h"

namespace mojo {
namespace switches {

// Comma separated list like:
// text/html,mojo:html_viewer,application/bravo,https://abarth.com/bravo
const char kContentHandlers[] = "content-handlers";

// In multiprocess mode, force these apps to be loaded in the main process.
// This is a comma-separated list of URLs. Example:
// --force-in-process=mojo:native_viewport_service,mojo:network_service
const char kForceInProcess[] = "force-in-process";

// Print the usage message and exit.
const char kHelp[] = "help";

// Specify origin to map to base url. See url_resolver.cc for details.
// Can be used multiple times.
const char kMapOrigin[] = "map-origin";

// Specifies a set of mappings to apply when resolving URLs. The value is a set
// of comma-separated mappings, where each mapping consists of a pair of URLs
// giving the to/from URLs to map. For example, 'a=b,c=d' contains two mappings,
// the first maps 'a' to 'b' and the second 'c' to 'd'.
const char kURLMappings[] = "url-mappings";

// When this is set, we create a temporary user data dir for the process, and
// add a flag so kUserDataDir points to it.
const char kUseTemporaryUserDataDir[] = "use-temporary-user-data-dir";

// Specifies the user data directory. This is the one directory which stores
// all persistent data.
const char kUserDataDir[] = "user-data-dir";

}  // namespace switches
}  // namespace mojo
