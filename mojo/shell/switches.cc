// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"

#include "base/basictypes.h"

namespace switches {

namespace {
// This controls logging verbosity. It's not strictly a switch for mojo_shell,
// and isn't included in the public switches, but is included here so that it
// doesn't trigger an error at startup.
const char kV[] = "v";

}  // namespace

// Specify configuration arguments for a Mojo application URL. For example:
// --args-for='mojo:wget http://www.google.com'
const char kArgsFor[] = "args-for";

// Used internally by the main process to indicate that a new process should be
// a child process. Not for user use.
const char kChildProcess[] = "child-process";

// Comma separated list like:
// text/html,mojo:html_viewer,application/bravo,https://abarth.com/bravo
const char kContentHandlers[] = "content-handlers";

// Force dynamically loaded apps / services to be loaded irrespective of cache
// instructions.
const char kDisableCache[] = "disable-cache";

// If set apps downloaded are not deleted.
const char kDontDeleteOnDownload[] = "dont-delete-on-download";

// Load apps in separate processes.
// TODO(vtl): Work in progress; doesn't work. Flip this to "disable" (or maybe
// change it to "single-process") when it works.
const char kEnableMultiprocess[] = "enable-multiprocess";

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

// If set apps downloaded are saved in with a predictable filename, to help
// remote debugging: when gdb is used through gdbserver, it needs to be able to
// find locally any loaded library. For this, gdb use the filename of the
// library. When using this flag, the application are named with the sha256 of
// their content.
const char kPredictableAppFilenames[] = "predictable-app-filenames";

// Starts tracing when the shell starts up, saving a trace file on disk after 5
// seconds or when the shell exits.
const char kTraceStartup[] = "trace-startup";

// Specifies a set of mappings to apply when resolving urls. The value is a set
// of ',' separated mappings, where each mapping consists of a pair of urls
// giving the to/from url to map. For example, 'a=b,c=d' contains two mappings,
// the first maps 'a' to 'b' and the second 'c' to 'd'.
const char kURLMappings[] = "url-mappings";

// Switches valid for the main process (i.e., that the user may pass in).
const char* kSwitchArray[] = {kV,
                              kArgsFor,
                              // |kChildProcess| not for user use.
                              kContentHandlers,
                              kDisableCache,
                              kDontDeleteOnDownload,
                              kEnableMultiprocess,
                              kForceInProcess,
                              kHelp,
                              kMapOrigin,
                              kOrigin,
                              kPredictableAppFilenames,
                              kTraceStartup,
                              kURLMappings};

const std::set<std::string> GetAllSwitches() {
  std::set<std::string> switch_set;

  for (size_t i = 0; i < arraysize(kSwitchArray); ++i)
    switch_set.insert(kSwitchArray[i]);
  return switch_set;
}

}  // namespace switches
