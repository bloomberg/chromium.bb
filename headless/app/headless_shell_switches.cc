// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/headless_shell_switches.h"

namespace headless {
namespace switches {

// The background color to be used if the page doesn't specify one. Provided as
// RGBA integer value in hex, e.g. 'ff0000ff' for red or '00000000' for
// transparent.
const char kDefaultBackgroundColor[] = "default-background-color";

// Enable crash reporter for headless.
const char kEnableCrashReporter[] = "enable-crash-reporter";

// Disable crash reporter for headless. It is enabled by default in official
// builds.
const char kDisableCrashReporter[] = "disable-crash-reporter";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Instructs headless_shell to cause network fetches to complete in order of
// creation. This removes a significant source of network related
// non-determinism at the cost of slower page loads.
const char kDeterministicFetch[] = "deterministic-fetch";

// Instructs headless_shell to print document.body.innerHTML to stdout.
const char kDumpDom[] = "dump-dom";

// Hide scrollbars from screenshots.
const char kHideScrollbars[] = "hide-scrollbars";

// Save a pdf file of the loaded page.
const char kPrintToPDF[] = "print-to-pdf";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored unless --proxy-server is also specified. This is a
// comma-separated list of bypass rules. See: "net/proxy/proxy_bypass_rules.h"
// for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[] = "proxy-server";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Should be used together with --remote-debugging-port.
// Note that the remote debugging protocol does not perform any authentication,
// so exposing it too widely can be a security risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// The given value is the fd of a socket already opened by the parent process.
// This allows automation to provide a listening socket for clients to connect
// to before chrome is fully fired up. In particular, a parent process can
// open the port, exec headles chrome, and connect to the devtools port
// immediately. Waiting for chrome to be ready is then handled by the first
// read from the port, which will block until chrome is ready. No polling is
// needed.
const char kRemoteDebuggingSocketFd[] = "remote-debugging-socket-fd";

// Runs a read-eval-print loop that allows the user to evaluate Javascript
// expressions.
const char kRepl[] = "repl";

// Save a screenshot of the loaded page.
const char kScreenshot[] = "screenshot";

// Causes SSL key material to be logged to the specified file for debugging
// purposes. See
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Key_Log_Format
// for the format.
const char kSSLKeyLogFile[] = "ssl-key-log-file";

// Issues a stop after the specified number of milliseconds.  This cancels all
// navigation and causes the DOMContentLoaded event to fire.
const char kTimeout[] = "timeout";

// Sets the GL implementation to use. Use a blank string to disable GL
// rendering.
const char kUseGL[] = "use-gl";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// Directory where the browser stores the user profile.
const char kUserDataDir[] = "user-data-dir";

// If set the system waits the specified number of virtual milliseconds before
// deeming the page to be ready.  For determinism virtual time does not advance
// while there are pending network fetches (i.e no timers will fire). Once all
// network fetches have completed, timers fire and if the system runs out of
// virtual time is fastforwarded so the next timer fires immediatley, until the
// specified virtual time budget is exhausted.
const char kVirtualTimeBudget[] = "virtual-time-budget";

// Sets the initial window size. Provided as string in the format "800,600".
const char kWindowSize[] = "window-size";

}  // namespace switches
}  // namespace headless
