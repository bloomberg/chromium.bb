// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_types.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

// This is the only supported cloud backend service for now.
const char kDriveCloudService[] = "drive";

// Error messages.
const char kNotSupportedService[] = "Cloud service %s not supported.";
const char kFileError[] = "File error %d.";

const BrowserThread::ID kControlThread = BrowserThread::UI;

}  // namespace

bool SyncFileSystemRequestFileSystemFunction::RunImpl() {
  std::string service_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &service_name));

  if (service_name != std::string(kDriveCloudService)) {
    error_ = base::StringPrintf(kNotSupportedService, service_name.c_str());
    return false;
  }

  // TODO(kinuko): Set up sync service, see if the user is signed in
  // and can access the service (i.e. Drive).

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetFileSystemContext();

  file_system_context->OpenSyncableFileSystem(
          service_name,
          source_url(),
          fileapi::kFileSystemTypeSyncable,
          true, /* create */
          base::Bind(
              &SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem,
              this));
  return true;
}

void SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem(
    base::PlatformFileError error,
    const std::string& file_system_name,
    const GURL& root_url) {
  DCHECK(BrowserThread::CurrentlyOn(kControlThread));
  if (error != base::PLATFORM_FILE_OK) {
    error_ = base::StringPrintf(kFileError, static_cast<int>(error));
    SendResponse(false);
    return;
  }

  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("name", file_system_name);
  dict->SetString("root", root_url.spec());
  SendResponse(true);
}

}  // namespace extensions
