// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/switches.h"

namespace switches {

// Used internally by the main process to indicate that a new process should be
// a child process. Takes the absolute path to the mojo application to load as
// an argument. Not for user use.
const char kChildProcess[] = "child-process";

// Comma separated list like:
// text/html,mojo:html_viewer,application/bravo,https://abarth.com/bravo
const char kContentHandlers[] = "content-handlers";

// Used internally to delete a loaded application after we load it. Used for
// transient applications. Not for user use.
const char kDeleteAfterLoad[] = "delete-after-load";

// Force dynamically loaded apps or services to be loaded irrespective of cache
// instructions.
const char kDisableCache[] = "disable-cache";

// Enables the sandbox on this process.
const char kEnableSandbox[] = "enable-sandbox";

// In multiprocess mode, force these apps to be loaded in the main process.
// This is a comma-separated list of URLs. Example:
// --force-in-process=mojo:native_viewport_service,mojo:network_service
const char kForceInProcess[] = "force-in-process";

// Print the usage message and exit.
const char kHelp[] = "help";

// Specify origin to map to base url. See url_resolver.cc for details.
// Can be used multiple times.
const char kMapOrigin[] = "map-origin";

// Starts tracing when the shell starts up, saving a trace file on disk after 5
// seconds or when the shell exits.
const char kTraceStartup[] = "trace-startup";

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
