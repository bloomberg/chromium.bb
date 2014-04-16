// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include "base/files/file.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Creates values to be passed to request events. These values can be extended
// by additional fields.
scoped_ptr<base::ListValue> CreateRequestValues(int file_system_id,
                                                int request_id) {
  scoped_ptr<base::ListValue> values(new base::ListValue());
  values->AppendInteger(file_system_id);
  values->AppendInteger(request_id);
  return values.Pass();
}

// Forwards the success callback to the status callback. Ignores arguments,
// since unmount request does not provide arguments.
void OnRequestUnmountSuccess(
    const fileapi::AsyncFileUtil::StatusCallback& callback,
    scoped_ptr<base::DictionaryValue> /* result */,
    bool /* has_next */) {
  callback.Run(base::File::FILE_OK);
}

}  // namespace

ProvidedFileSystem::ProvidedFileSystem(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info)
    : event_router_(event_router), file_system_info_(file_system_info) {}

ProvidedFileSystem::~ProvidedFileSystem() {}

bool ProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  int request_id = request_manager_.CreateRequest(
      base::Bind(&OnRequestUnmountSuccess, callback), callback);

  if (!request_id)
    return false;

  scoped_ptr<base::ListValue> values(
      CreateRequestValues(file_system_info_.file_system_id(), request_id));

  event_router_->DispatchEventToExtension(
      file_system_info_.extension_id(),
      make_scoped_ptr(new extensions::Event(
          extensions::api::file_system_provider::OnUnmountRequested::kEventName,
          values.Pass())));

  return true;
}

const ProvidedFileSystemInfo& ProvidedFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

RequestManager* ProvidedFileSystem::GetRequestManager() {
  return &request_manager_;
}

}  // namespace file_system_provider
}  // namespace chromeos
