// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

namespace gdata {

GDataSyncClient::GDataSyncClient()
    : file_system_(NULL)  {
}

GDataSyncClient::~GDataSyncClient() {
  if (file_system_)
    file_system_->RemoveObserver(this);
}

void GDataSyncClient::Start(GDataFileSystem* file_system) {
  file_system_ = file_system;
  file_system_->AddObserver(this);
}

void GDataSyncClient::OnFilePinned(const std::string& resource_id,
                                   const std::string& md5) {
  // TODO(satorux): As of now, "available offline" column in the file browser
  // is not clickable and the user action is not propagated to the
  // GDataFileSystem.  crosbug.com/27961
  LOG(WARNING) << "Not implemented";
}

}  // namespace gdata
