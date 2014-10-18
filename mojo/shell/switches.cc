// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"

namespace switches {

// Specify configuration arguments for a Mojo application URL. For example:
// --args-for='mojo://wget http://www.google.com'
const char kArgsFor[] = "args-for";

// Used to specify the type of child process (switch values from
// |ChildProcess::Type|).
const char kChildProcessType[] = "child-process-type";

// Comma separated list like:
// text/html,mojo://html_viewer,application/bravo,https://abarth.com/bravo
const char kContentHandlers[] = "content-handlers";

// Force dynamically loaded apps / services to be loaded irrespective of cache
// instructions.
const char kDisableCache[] = "disable-cache";

// Allow externally-running applications to discover, connect to, and register
// themselves with the shell.
// TODO(cmasone): Work in progress. Once we're sure this works, remove.
const char kEnableExternalApplications[] = "enable-external-applications";

// Load apps in separate processes.
// TODO(vtl): Work in progress; doesn't work. Flip this to "disable" (or maybe
// change it to "single-process") when it works.
const char kEnableMultiprocess[] = "enable-multiprocess";

// Print the usage message and exit.
const char kHelp[] = "help";

// Map mojo: URLs to a shared library of similar name at this origin. See
// mojo_url_resolver.cc for details.
const char kOrigin[] = "origin";

// Enables the mojo spy, which acts as a man-in-the-middle inspector for
// message pipes and other activities. This is work in progress.
const char kSpy[] = "spy";

// Specifies a set of mappings to apply when resolving urls. The value is set of
// ',' separated mappings, where each mapping consists of a pair of urls giving
// the to/from url to map. For example, 'a=b,c=d' contains two mappings, the
// first maps 'a' to 'b' and the second 'c' to 'd'.
const char kURLMappings[] = "url-mappings";

}  // namespace switches
