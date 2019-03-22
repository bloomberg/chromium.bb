// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_win.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/win/windows_version.h"
#include "content/public/browser/web_ui.h"

namespace {

// Return the marketing version of Windows OS, this may return an empty string
// if values returned by base::win::OSinfo are not defined below.
std::string FullWindowsVersion() {
  std::string version;
  base::win::OSInfo* gi = base::win::OSInfo::GetInstance();
  const int major = gi->version_number().major;
  const int minor = gi->version_number().minor;
  const int build = gi->version_number().build;
  const int patch = gi->version_number().patch;
  // Server or Desktop
  const bool server =
      gi->version_type() == base::win::VersionType::SUITE_SERVER;
  // Service Pack
  const std::string sp = gi->service_pack_str();

  if (major == 10) {
    version += (server) ? "Server OS" : "10 OS";
  } else if (major == 6) {
    switch (minor) {
      case 0:
        // Windows Vista or Server 2008
        version += (server) ? "Server 2008 " : "Vista ";
        version += sp;
        break;
      case 1:
        // Windows 7 or Server 2008 R2
        version += (server) ? "Server 2008 R2 " : "7 ";
        version += sp;
        break;
      case 2:
        // Windows 8 or Server 2012
        version += (server) ? "Server 2012" : "8";
        break;
      case 3:
        // Windows 8.1 or Server 2012 R2
        version += (server) ? "Server 2012 R2" : "8.1";
        break;
      default:
        // unknown version
        return base::StringPrintf("unknown version 6.%d", minor);
    }
  } else if ((major == 5) && (minor > 0)) {
    // Windows XP or Server 2003
    version += (server) ? "Server 2003 " : "XP ";
    version += sp;
  } else {
    // unknown version
    return base::StringPrintf("unknown version %d.%d", major, minor);
  }

  const std::string release_id = gi->release_id();

  if (!release_id.empty())
    version += " Version " + release_id;

  if (patch > 0)
    version += base::StringPrintf(" (Build %d.%d)", build, patch);
  else
    version += base::StringPrintf(" (Build %d)", build);
  return version;
}

}  // namespace

VersionHandlerWindows::VersionHandlerWindows() : weak_factory_(this) {}

VersionHandlerWindows::~VersionHandlerWindows() {}

void VersionHandlerWindows::HandleRequestVersionInfo(
    const base::ListValue* args) {
  // Start the asynchronous load of the versions.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FullWindowsVersion),
      base::BindOnce(&VersionHandlerWindows::OnVersion,
                     weak_factory_.GetWeakPtr()));

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerWindows::OnVersion(const std::string& version) {
  base::Value arg(version);
  CallJavascriptFunction("returnOsVersion", arg);
}

// static
std::string VersionHandlerWindows::GetFullWindowsVersionForTesting() {
  return FullWindowsVersion();
}
