// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_CHROMEOS)

#include "chrome/browser/webui/options/advanced_options_utils.h"

#include "base/environment.h"
#include "base/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/string_tokenizer.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/process_watcher.h"
#include "ui/base/gtk/gtk_signal.h"

// Command used to configure GNOME proxy settings. The command was renamed
// in January 2009, so both are used to work on both old and new systems.
const char* kOldGNOMEProxyConfigCommand[] = {"gnome-network-preferences", NULL};
const char* kGNOMEProxyConfigCommand[] = {"gnome-network-properties", NULL};
// KDE3 and KDE4 are only slightly different, but incompatible. Go figure.
const char* kKDE3ProxyConfigCommand[] = {"kcmshell", "proxy", NULL};
const char* kKDE4ProxyConfigCommand[] = {"kcmshell4", "proxy", NULL};

// The URL for Linux proxy configuration help when not running under a
// supported desktop environment.
const char kLinuxProxyConfigUrl[] = "about:linux-proxy-config";

struct ProxyConfigCommand {
  std::string binary;
  const char** argv;
};

namespace {

// Search $PATH to find one of the commands. Store the full path to
// it in the |binary| field and the command array index in in |index|.
bool SearchPATH(ProxyConfigCommand* commands, size_t ncommands, size_t* index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const char* path = getenv("PATH");
  if (!path)
    return false;
  FilePath bin_path;
  CStringTokenizer tk(path, path + strlen(path), ":");
  // Search $PATH looking for the commands in order.
  while (tk.GetNext()) {
    for (size_t i = 0; i < ncommands; i++) {
      bin_path = FilePath(tk.token()).Append(commands[i].argv[0]);
      if (file_util::PathExists(bin_path)) {
        commands[i].binary = bin_path.value();
        if (index)
          *index = i;
        return true;
      }
    }
  }
  // Did not find any of the binaries in $PATH.
  return false;
}

// Show the proxy config URL in the given tab.
void ShowLinuxProxyConfigUrl(TabContents* tab_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::Environment> env(base::Environment::Create());
  const char* name = base::nix::GetDesktopEnvironmentName(env.get());
  if (name)
    LOG(ERROR) << "Could not find " << name << " network settings in $PATH";
  tab_contents->OpenURL(GURL(kLinuxProxyConfigUrl), GURL(),
                        NEW_FOREGROUND_TAB, PageTransition::LINK);
}

// Start the given proxy configuration utility.
void StartProxyConfigUtil(TabContents* tab_contents,
                          const ProxyConfigCommand& command) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<std::string> argv;
  argv.push_back(command.binary);
  for (size_t i = 1; command.argv[i]; i++)
    argv.push_back(command.argv[i]);
  base::file_handle_mapping_vector no_files;
  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_files, false, &handle)) {
    LOG(ERROR) << "StartProxyConfigUtil failed to start " << command.binary;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&ShowLinuxProxyConfigUrl, tab_contents));
    return;
  }
  ProcessWatcher::EnsureProcessGetsReaped(handle);
}

// Detect, and if possible, start the appropriate proxy config utility. On
// failure to do so, show the Linux proxy config URL in a new tab instead.
void DetectAndStartProxyConfigUtil(TabContents* tab_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<base::Environment> env(base::Environment::Create());

  ProxyConfigCommand command;
  bool found_command = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME: {
      size_t index;
      ProxyConfigCommand commands[2];
      commands[0].argv = kGNOMEProxyConfigCommand;
      commands[1].argv = kOldGNOMEProxyConfigCommand;
      found_command = SearchPATH(commands, 2, &index);
      if (found_command)
        command = commands[index];
      break;
    }

    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
      command.argv = kKDE3ProxyConfigCommand;
      found_command = SearchPATH(&command, 1, NULL);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      command.argv = kKDE4ProxyConfigCommand;
      found_command = SearchPATH(&command, 1, NULL);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }

  if (found_command) {
    StartProxyConfigUtil(tab_contents, command);
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&ShowLinuxProxyConfigUrl, tab_contents));
  }
}

}  // anonymous namespace

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
    TabContents* tab_contents) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&DetectAndStartProxyConfigUtil, tab_contents));
}

#endif  // !defined(OS_CHROMEOS)
