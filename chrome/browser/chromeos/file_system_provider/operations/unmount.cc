// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"

#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

Unmount::Unmount(extensions::EventRouter* event_router,
                 const ProvidedFileSystemInfo& file_system_info,
                 const fileapi::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info), callback_(callback) {
}

Unmount::~Unmount() {
}

bool Unmount::Execute(int request_id) {
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnUnmountRequested::kEventName,
      values.Pass());
}

void Unmount::OnSuccess(int /* request_id */,
                        scoped_ptr<RequestValue> /* result */,
                        bool /* has_more */) {
  callback_.Run(base::File::FILE_OK);
}

void Unmount::OnError(int /* request_id */,
                      scoped_ptr<RequestValue> /* result */,
                      base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
