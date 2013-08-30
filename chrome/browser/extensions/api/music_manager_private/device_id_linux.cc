// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/music_manager_private/device_id.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kDBusFilename[] = "/var/lib/dbus/machine-id";

void GetDBusMachineId(const extensions::api::DeviceId::IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string result;
  if (!base::ReadFileToString(base::FilePath(kDBusFilename), &result)) {
    DLOG(WARNING) << "Error reading dbus machine id file.";
    result = "";
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

}

namespace extensions {
namespace api {

// Linux: Use the content of the "DBus" machine-id file.
/* static */
void DeviceId::GetMachineId(const IdCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(GetDBusMachineId, callback));
}

}  // namespace api
}  // namespace extensions
