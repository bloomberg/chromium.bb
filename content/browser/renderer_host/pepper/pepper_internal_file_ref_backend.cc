// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_internal_file_ref_backend.h"

#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_util_proxy.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/file_ref_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "webkit/browser/fileapi/file_permission_policy.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_util.h"

using ppapi::host::PpapiHost;
using ppapi::host::ResourceHost;

namespace content {

PepperInternalFileRefBackend::PepperInternalFileRefBackend(
    PpapiHost* host,
    int render_process_id,
    base::WeakPtr<PepperFileSystemBrowserHost> fs_host,
    const std::string& path) : host_(host),
                               render_process_id_(render_process_id),
                               fs_host_(fs_host),
                               fs_type_(fs_host->GetType()),
                               path_(path),
                               weak_factory_(this) {
  ppapi::NormalizeInternalPath(&path_);
}

PepperInternalFileRefBackend::~PepperInternalFileRefBackend() {
}

fileapi::FileSystemURL PepperInternalFileRefBackend::GetFileSystemURL() const {
  if (!fs_url_.is_valid() && fs_host_.get()) {
    GURL fs_path = fs_host_->GetRootUrl().Resolve(
        net::EscapePath(path_.substr(1)));
    fs_url_ = GetFileSystemContext()->CrackURL(fs_path);
  }
  return fs_url_;
}

scoped_refptr<fileapi::FileSystemContext>
PepperInternalFileRefBackend::GetFileSystemContext() const {
  // TODO(teravest): Make this work for CRX file systems.
  if (!fs_host_.get())
    return NULL;
  return fs_host_->GetFileSystemContext();
}

void PepperInternalFileRefBackend::DidFinish(
    ppapi::host::ReplyMessageContext context,
    const IPC::Message& msg,
    base::PlatformFileError error) {
  context.params.set_result(ppapi::PlatformFileErrorToPepperError(error));
  host_->SendReply(context, msg);
}

int32_t PepperInternalFileRefBackend::MakeDirectory(
    ppapi::host::ReplyMessageContext reply_context,
    bool make_ancestors) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::PlatformFileError error;
  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kCreateFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  GetFileSystemContext()->operation_runner()->CreateDirectory(
      GetFileSystemURL(),
      false,
      make_ancestors,
      base::Bind(&PepperInternalFileRefBackend::DidFinish,
                 weak_factory_.GetWeakPtr(),
                 reply_context,
                 PpapiPluginMsg_FileRef_MakeDirectoryReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Touch(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  // TODO(teravest): Change this to be kWriteFilePermissions here and in
  // fileapi_message_filter.
  base::PlatformFileError error;
  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kCreateFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  GetFileSystemContext()->operation_runner()->TouchFile(
      GetFileSystemURL(),
      ppapi::PPTimeToTime(last_access_time),
      ppapi::PPTimeToTime(last_modified_time),
      base::Bind(&PepperInternalFileRefBackend::DidFinish,
                 weak_factory_.GetWeakPtr(),
                 reply_context,
                 PpapiPluginMsg_FileRef_TouchReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Delete(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::PlatformFileError error;
  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kWriteFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  GetFileSystemContext()->operation_runner()->Remove(
      GetFileSystemURL(),
      false,
      base::Bind(&PepperInternalFileRefBackend::DidFinish,
                 weak_factory_.GetWeakPtr(),
                 reply_context,
                 PpapiPluginMsg_FileRef_DeleteReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Rename(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Resource new_file_ref) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::PlatformFileError error;
  if (!HasPermissionsForFile(
      GetFileSystemURL(),
      fileapi::kReadFilePermissions | fileapi::kWriteFilePermissions,
      &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  ResourceHost* resource_host = host_->GetResourceHost(new_file_ref);
  if (!resource_host)
    return PP_ERROR_BADRESOURCE;

  PepperFileRefHost* file_ref_host = resource_host->AsPepperFileRefHost();
  if (!file_ref_host)
    return PP_ERROR_BADRESOURCE;

  fileapi::FileSystemURL new_url = file_ref_host->GetFileSystemURL();
  if (!new_url.is_valid())
    return PP_ERROR_FAILED;
  if (!new_url.IsInSameFileSystem(GetFileSystemURL()))
    return PP_ERROR_FAILED;

  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kCreateFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }


  GetFileSystemContext()->operation_runner()->Move(
      GetFileSystemURL(),
      new_url,
      base::Bind(&PepperInternalFileRefBackend::DidFinish,
                 weak_factory_.GetWeakPtr(),
                 reply_context,
                 PpapiPluginMsg_FileRef_RenameReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Query(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::PlatformFileError error;
  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kReadFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  GetFileSystemContext()->operation_runner()->GetMetadata(
      GetFileSystemURL(),
      base::Bind(&PepperInternalFileRefBackend::GetMetadataComplete,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
  return PP_OK_COMPLETIONPENDING;
}

void PepperInternalFileRefBackend::GetMetadataComplete(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  reply_context.params.set_result(ppapi::PlatformFileErrorToPepperError(error));

  PP_FileInfo pp_file_info;
  if (error == base::PLATFORM_FILE_OK)
    ppapi::PlatformFileInfoToPepperFileInfo(file_info, fs_type_, &pp_file_info);
  else
    memset(&pp_file_info, 0, sizeof(pp_file_info));

  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileRef_QueryReply(pp_file_info));
}

int32_t PepperInternalFileRefBackend::ReadDirectoryEntries(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::PlatformFileError error;
  if (!HasPermissionsForFile(GetFileSystemURL(),
                             fileapi::kReadFilePermissions,
                             &error)) {
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  GetFileSystemContext()->operation_runner()->ReadDirectory(
      GetFileSystemURL(),
      base::Bind(&PepperInternalFileRefBackend::ReadDirectoryComplete,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
  return PP_OK_COMPLETIONPENDING;
}

void PepperInternalFileRefBackend::ReadDirectoryComplete(
    ppapi::host::ReplyMessageContext context,
    base::PlatformFileError error,
    const fileapi::FileSystemOperation::FileEntryList& file_list,
    bool has_more) {
  // The current filesystem backend always returns false.
  DCHECK(!has_more);

  context.params.set_result(ppapi::PlatformFileErrorToPepperError(error));

  std::vector<ppapi::FileRef_CreateInfo> infos;
  std::vector<PP_FileType> file_types;
  if (error == base::PLATFORM_FILE_OK && fs_host_.get()) {
    std::string dir_path = path_;
    if (dir_path.empty() || dir_path[dir_path.size() - 1] != '/')
      dir_path += '/';

    for (fileapi::FileSystemOperation::FileEntryList::const_iterator it =
         file_list.begin(); it != file_list.end(); ++it) {
      if (it->is_directory)
        file_types.push_back(PP_FILETYPE_DIRECTORY);
      else
        file_types.push_back(PP_FILETYPE_REGULAR);

      ppapi::FileRef_CreateInfo info;
      info.file_system_type = fs_type_;
      info.file_system_plugin_resource = fs_host_->pp_resource();
      std::string path =
          dir_path + fileapi::FilePathToString(base::FilePath(it->name));
      info.internal_path = path;
      info.display_name = ppapi::GetNameForInternalFilePath(path);
      infos.push_back(info);
    }
  }

  host_->SendReply(context,
      PpapiPluginMsg_FileRef_ReadDirectoryEntriesReply(infos, file_types));
}

int32_t PepperInternalFileRefBackend::GetAbsolutePath(
    ppapi::host::ReplyMessageContext reply_context) {
  host_->SendReply(reply_context,
      PpapiPluginMsg_FileRef_GetAbsolutePathReply(path_));
  return PP_OK_COMPLETIONPENDING;
}

bool PepperInternalFileRefBackend::HasPermissionsForFile(
    const fileapi::FileSystemURL& url,
    int permissions,
    base::PlatformFileError* error) const {
  return CheckFileSystemPermissionsForProcess(GetFileSystemContext(),
                                              render_process_id_,
                                              url,
                                              permissions,
                                              error);
}

}  // namespace content
