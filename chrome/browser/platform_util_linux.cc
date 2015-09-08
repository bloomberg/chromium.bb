// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace platform_util {

namespace {

void XDGUtil(const std::string& util,
             const base::FilePath& working_directory,
             const std::string& arg) {
  std::vector<std::string> argv;
  argv.push_back(util);
  argv.push_back(arg);

  base::LaunchOptions options;
  options.current_directory = working_directory;
  options.allow_new_privs = true;
  // xdg-open can fall back on mailcap which eventually might plumb through
  // to a command that needs a terminal.  Set the environment variable telling
  // it that we definitely don't have a terminal available and that it should
  // bring up a new terminal if necessary.  See "man mailcap".
  options.environ["MM_NOTTTY"] = "1";

  // In Google Chrome, we do not let GNOME's bug-buddy intercept our crashes.
  // However, we do not want this environment variable to propagate to external
  // applications. See http://crbug.com/24120
  char* disable_gnome_bug_buddy = getenv("GNOME_DISABLE_CRASH_DIALOG");
  if (disable_gnome_bug_buddy &&
      disable_gnome_bug_buddy == std::string("SET_BY_GOOGLE_CHROME"))
    options.environ["GNOME_DISABLE_CRASH_DIALOG"] = std::string();

  base::Process process = base::LaunchProcess(argv, options);
  if (process.IsValid())
    base::EnsureProcessGetsReaped(process.Pid());
}

void XDGOpen(const base::FilePath& working_directory, const std::string& path) {
  XDGUtil("xdg-open", working_directory, path);
}

void XDGEmail(const std::string& email) {
  XDGUtil("xdg-email", base::FilePath(), email);
}

}  // namespace

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  switch (type) {
    case OPEN_FILE:
      XDGOpen(path.DirName(), path.value());
      break;
    case OPEN_FOLDER:
      // The utility process checks the working directory prior to the
      // invocation of xdg-open by changing the current directory into it. This
      // operation only succeeds if |path| is a directory. Opening "." from
      // there ensures that the target of the operation is a directory.  Note
      // that there remains a TOCTOU race where the directory could be unlinked
      // between the time the utility process changes into the directory and the
      // time the application invoked by xdg-open inspects the path by name.
      XDGOpen(path, ".");
      break;
  }
}
}  // namespace internal

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(estade): It would be nice to be able to select the file in the file
  // manager, but that probably requires extending xdg-open. For now just show
  // the folder.
  OpenItem(profile, full_path.DirName(), OPEN_FOLDER, OpenOperationCallback());
}

void OpenExternal(Profile* profile, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url.SchemeIs("mailto"))
    XDGEmail(url.spec());
  else
    XDGOpen(base::FilePath(), url.spec());
}

}  // namespace platform_util
