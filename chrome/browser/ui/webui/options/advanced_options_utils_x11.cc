// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_CHROMEOS)

#include "chrome/browser/ui/webui/options/advanced_options_utils.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace options {

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
void ShowLinuxProxyConfigUrl(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::Environment> env(base::Environment::Create());
  const char* name = base::nix::GetDesktopEnvironmentName(env.get());
  if (name)
    LOG(ERROR) << "Could not find " << name << " network settings in $PATH";
  OpenURLParams params(
      GURL(kLinuxProxyConfigUrl), Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (web_contents)
    web_contents->OpenURL(params);
}

// Start the given proxy configuration utility.
bool StartProxyConfigUtil(const char* command[]) {
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
    base::FilePath file(paths[i]);
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
void DetectAndStartProxyConfigUtil(int render_process_id,
                                   int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<base::Environment> env(base::Environment::Create());

  bool launched = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY: {
      launched = StartProxyConfigUtil(kGNOME2ProxyConfigCommand);
      if (!launched) {
        // We try this second, even though it's the newer way, because this
        // command existed in older versions of GNOME, but it didn't do the
        // same thing. The older command is gone though, so this should do
        // the right thing. (Also some distributions have blurred the lines
        // between GNOME 2 and 3, so we can't necessarily detect what the
        // right thing is based on indications of which version we have.)
        launched = StartProxyConfigUtil(kGNOME3ProxyConfigCommand);
      }
      break;
    }

    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
      launched = StartProxyConfigUtil(kKDE3ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      launched = StartProxyConfigUtil(kKDE4ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }

  if (launched)
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowLinuxProxyConfigUrl, render_process_id, render_view_id));
}

}  // anonymous namespace

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
    WebContents* web_contents) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&DetectAndStartProxyConfigUtil,
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetRenderViewHost()->GetRoutingID()));
}

}  // namespace options

#endif  // !defined(OS_CHROMEOS)
