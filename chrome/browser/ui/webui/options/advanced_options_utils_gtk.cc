// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_CHROMEOS)

#include "chrome/browser/ui/webui/options/advanced_options_utils.h"

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/process_watcher.h"

// Command used to configure GNOME proxy settings. The command was renamed
// in January 2009, so both are used to work on both old and new systems.
// As on April 2011, many systems do not have the old command anymore.
// TODO(thestig) Remove the old command in the future.
const char* kOldGNOMEProxyConfigCommand[] = {"gnome-network-preferences", NULL};
const char* kGNOMEProxyConfigCommand[] = {"gnome-network-properties", NULL};
// KDE3 and KDE4 are only slightly different, but incompatible. Go figure.
const char* kKDE3ProxyConfigCommand[] = {"kcmshell", "proxy", NULL};
const char* kKDE4ProxyConfigCommand[] = {"kcmshell4", "proxy", NULL};

// The URL for Linux proxy configuration help when not running under a
// supported desktop environment.
const char kLinuxProxyConfigUrl[] = "about:linux-proxy-config";

namespace {

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
bool StartProxyConfigUtil(TabContents* tab_contents, const char* command[]) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<std::string> argv;
  for (size_t i = 0; command[i]; ++i)
    argv.push_back(command[i]);
  base::file_handle_mapping_vector no_files;
  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_files, false, &handle)) {
    LOG(ERROR) << "StartProxyConfigUtil failed to start " << command[0];
    return false;
  }
  ProcessWatcher::EnsureProcessGetsReaped(handle);
  return true;
}

// Detect, and if possible, start the appropriate proxy config utility. On
// failure to do so, show the Linux proxy config URL in a new tab instead.
void DetectAndStartProxyConfigUtil(TabContents* tab_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<base::Environment> env(base::Environment::Create());

  bool launched = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME: {
      launched = StartProxyConfigUtil(tab_contents, kGNOMEProxyConfigCommand);
      if (!launched) {
        launched = StartProxyConfigUtil(tab_contents,
                                        kOldGNOMEProxyConfigCommand);
      }
      break;
    }

    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
      launched = StartProxyConfigUtil(tab_contents, kKDE3ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      launched = StartProxyConfigUtil(tab_contents, kKDE4ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }

  if (launched)
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&ShowLinuxProxyConfigUrl, tab_contents));
}

}  // anonymous namespace

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
    TabContents* tab_contents) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&DetectAndStartProxyConfigUtil, tab_contents));
}

#endif  // !defined(OS_CHROMEOS)
