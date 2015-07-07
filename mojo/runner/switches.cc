// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/switches.h"

#include "base/basictypes.h"

namespace switches {

// Used just for debugging to make it easier to attach debuggers. The actual app
// path that is used is sent over IPC.
const char kApp[] = "app";

// Used internally by the main process to indicate that a new process should be
// a child process. Not for user use.
const char kChildProcess[] = "child-process";

// Comma separated list like:
// text/html,mojo:html_viewer,application/bravo,https://abarth.com/bravo
const char kContentHandlers[] = "content-handlers";

// Force dynamically loaded apps / services to be loaded irrespective of cache
// instructions.
const char kDisableCache[] = "disable-cache";

// In multiprocess mode, force these apps to be loaded in the main process.
// Comma-separate list of URLs. Example:
// --force-in-process=mojo:native_viewport_service,mojo:network_service
const char kForceInProcess[] = "force-in-process";

// Print the usage message and exit.
const char kHelp[] = "help";

// Specify origin to map to base url. See url_resolver.cc for details.
// Can be used multiple times.
const char kMapOrigin[] = "map-origin";

// Map mojo: URLs to a shared library of similar name at this origin. See
// url_resolver.cc for details.
const char kOrigin[] = "origin";

// Starts tracing when the shell starts up, saving a trace file on disk after 5
// seconds or when the shell exits.
const char kTraceStartup[] = "trace-startup";

// Specifies a set of mappings to apply when resolving urls. The value is a set
// of ',' separated mappings, where each mapping consists of a pair of urls
// giving the to/from url to map. For example, 'a=b,c=d' contains two mappings,
// the first maps 'a' to 'b' and the second 'c' to 'd'.
const char kURLMappings[] = "url-mappings";

}  // namespace switches
