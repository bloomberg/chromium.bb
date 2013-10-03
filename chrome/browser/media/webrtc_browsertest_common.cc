// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_common.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "content/public/test/browser_test_utils.h"

const int kDefaultPollIntervalMsec = 250;

bool SleepInJavascript(content::WebContents* tab_contents, int timeout_msec) {
  const std::string javascript = base::StringPrintf(
      "setTimeout(function() {"
      "  window.domAutomationController.send('sleep-ok');"
      "}, %d)", timeout_msec);

  std::string result;
  bool ok = content::ExecuteScriptAndExtractString(
      tab_contents, javascript, &result);
  return ok && result == "sleep-ok";
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents) {
  return PollingWaitUntil(javascript, evaluates_to, tab_contents,
                          kDefaultPollIntervalMsec);
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents,
                      int poll_interval_msec) {
  base::Time start_time = base::Time::Now();
  base::TimeDelta timeout = TestTimeouts::action_max_timeout();
  std::string result;

  while (base::Time::Now() - start_time < timeout) {
    std::string result;
    if (!content::ExecuteScriptAndExtractString(tab_contents, javascript,
                                                &result)) {
      LOG(ERROR) << "Failed to execute javascript " << javascript;
      return false;
    }

    if (evaluates_to == result)
      return true;

    // Sleep a bit here to keep this loop from spinlocking too badly.
    if (!SleepInJavascript(tab_contents, poll_interval_msec)) {
      // TODO(phoglund): Figure out why this fails every now and then.
      // It's not a huge deal if it does though.
      LOG(ERROR) << "Failed to sleep.";
    }
  }
  LOG(ERROR) << "Timed out while waiting for " << javascript <<
      " to evaluate to " << evaluates_to << "; last result was '" << result <<
      "'";
  return false;
}

static base::FilePath::CharType kServerExecutable[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("peerconnection_server.exe");
#else
    FILE_PATH_LITERAL("peerconnection_server");
#endif

const char PeerConnectionServerRunner::kDefaultPort[] = "7778";

bool PeerConnectionServerRunner::Start() {
  base::FilePath peerconnection_server;
  if (!PathService::Get(base::DIR_MODULE, &peerconnection_server)) {
    LOG(ERROR) << "Failed retrieving base::DIR_MODULE!";
    return false;
  }
  peerconnection_server = peerconnection_server.Append(kServerExecutable);

  if (!base::PathExists(peerconnection_server)) {
    LOG(ERROR) << "Missing " << kServerExecutable << ". You must build "
        "it so it ends up next to the browser test binary.";
    return false;
  }

  CommandLine command_line(peerconnection_server);
  command_line.AppendSwitchASCII("port", kDefaultPort);
  LOG(INFO) << "Running " << command_line.GetCommandLineString();
  return base::LaunchProcess(command_line,
                             base::LaunchOptions(),
                             &server_pid_);
}

bool PeerConnectionServerRunner::Stop() {
  return base::KillProcess(server_pid_, 0, false);
}

void PeerConnectionServerRunner::KillAllPeerConnectionServersOnCurrentSystem() {
  if (!base::KillProcesses(kServerExecutable, -1, NULL)) {
    LOG(ERROR) << "Failed to kill instances of " << kServerExecutable << ".";
    return;
  }
  base::WaitForProcessesToExit(kServerExecutable,
                               base::TimeDelta::FromSeconds(5), NULL);
}
