// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_CHROMEOS)

#include "chrome/browser/ui/webui/options/advanced_options_utils.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Command used to configure GNOME 2 proxy settings.
const char* kGNOME2ProxyConfigCommand[] = {"gnome-network-properties", NULL};
// In GNOME 3, we might need to run gnome-control-center instead. We try this
// only after gnome-network-properties is not found, because older GNOME also
// has this but it doesn't do the same thing. See below where we use it.
const char* kGNOME3ProxyConfigCommand[] = {"gnome-control-center", "network",
                                           NULL};
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
                        NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
}

// Start the given proxy configuration utility.
bool StartProxyConfigUtil(TabContents* tab_contents, const char* command[]) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // base::LaunchProcess() returns true ("success") if the fork()
  // succeeds, but not necessarily the exec(). We'd like to be able to
  // use StartProxyConfigUtil() to search possible options and stop on
  // success, so we search $PATH first to predict whether the exec is
  // expected to succeed.
  // TODO(mdm): this is a useful check, and is very similar to some
  // code in proxy_config_service_linux.cc. It should probably be in
  // base:: somewhere.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string path;
  if (!env->GetVar("PATH", &path)) {
    LOG(ERROR) << "No $PATH variable. Assuming no " << command[0] << ".";
    return false;
  }
  std::vector<std::string> paths;
  Tokenize(path, ":", &paths);
  bool found = false;
  for (size_t i = 0; i < paths.size(); ++i) {
    FilePath file(paths[i]);
    if (file_util::PathExists(file.Append(command[0]))) {
      found = true;
      break;
    }
  }
  if (!found)
    return false;
  std::vector<std::string> argv;
  for (size_t i = 0; command[i]; ++i)
    argv.push_back(command[i]);
  base::ProcessHandle handle;
  if (!base::LaunchProcess(argv, base::LaunchOptions(), &handle)) {
    LOG(ERROR) << "StartProxyConfigUtil failed to start " << command[0];
    return false;
  }
  base::EnsureProcessGetsReaped(handle);
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
      launched = StartProxyConfigUtil(tab_contents, kGNOME2ProxyConfigCommand);
      if (!launched) {
        // We try this second, even though it's the newer way, because this
        // command existed in older versions of GNOME, but it didn't do the
        // same thing. The older command is gone though, so this should do
        // the right thing. (Also some distributions have blurred the lines
        // between GNOME 2 and 3, so we can't necessarily detect what the
        // right thing is based on indications of which version we have.)
        launched = StartProxyConfigUtil(tab_contents,
                                        kGNOME3ProxyConfigCommand);
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
      base::Bind(&ShowLinuxProxyConfigUrl, tab_contents));
}

}  // anonymous namespace

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
    TabContents* tab_contents) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&DetectAndStartProxyConfigUtil, tab_contents));
}

#endif  // !defined(OS_CHROMEOS)
