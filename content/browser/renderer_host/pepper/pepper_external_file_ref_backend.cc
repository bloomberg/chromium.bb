// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_external_file_ref_backend.h"

#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"

namespace content {

PepperExternalFileRefBackend::PepperExternalFileRefBackend(
    ppapi::host::PpapiHost* host,
    int render_process_id,
    const base::FilePath& path) : host_(host),
                                  path_(path),
                                  render_process_id_(render_process_id),
                                  weak_factory_(this) {
  task_runner_ =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
}

PepperExternalFileRefBackend::~PepperExternalFileRefBackend() {
}

int32_t PepperExternalFileRefBackend::MakeDirectory(
    ppapi::host::ReplyMessageContext reply_context,
    bool make_ancestors) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Touch(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  IPC::Message reply_msg = PpapiPluginMsg_FileRef_TouchReply();
  base::FileUtilProxy::Touch(
      task_runner_.get(),
      path_,
      ppapi::PPTimeToTime(last_access_time),
      ppapi::PPTimeToTime(last_modified_time),
      base::Bind(&PepperExternalFileRefBackend::DidFinish,
                 weak_factory_.GetWeakPtr(),
                 reply_context,
                 reply_msg));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperExternalFileRefBackend::Delete(
    ppapi::host::ReplyMessageContext reply_context) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Rename(
    ppapi::host::ReplyMessageContext reply_context,
    PepperFileRefHost* new_file_ref) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Query(
    ppapi::host::ReplyMessageContext reply_context) {
  bool ok = base::FileUtilProxy::GetFileInfo(
      task_runner_.get(),
      path_,
      base::Bind(&PepperExternalFileRefBackend::GetMetadataComplete,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
  DCHECK(ok);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperExternalFileRefBackend::ReadDirectoryEntries(
    ppapi::host::ReplyMessageContext context) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::GetAbsolutePath(
    ppapi::host::ReplyMessageContext reply_context) {
  host_->SendReply(reply_context,
      PpapiPluginMsg_FileRef_GetAbsolutePathReply(path_.AsUTF8Unsafe()));
  return PP_OK;
}

fileapi::FileSystemURL PepperExternalFileRefBackend::GetFileSystemURL() const {
  return fileapi::FileSystemURL();
}

std::string PepperExternalFileRefBackend::GetFileSystemURLSpec() const {
  return std::string();
}

base::FilePath PepperExternalFileRefBackend::GetExternalPath() const {
  return path_;
}

int32_t PepperExternalFileRefBackend::CanRead() const {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          CanReadFile(render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperExternalFileRefBackend::CanWrite() const {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          CanWriteFile(render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperExternalFileRefBackend::CanCreate() const {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          CanCreateFile(render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperExternalFileRefBackend::CanReadWrite() const {
  ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadFile(render_process_id_, path_) ||
      !policy->CanWriteFile(render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

void PepperExternalFileRefBackend::DidFinish(
    ppapi::host::ReplyMessageContext reply_context,
    const IPC::Message& msg,
    base::PlatformFileError error) {
  reply_context.params.set_result(ppapi::PlatformFileErrorToPepperError(error));
  host_->SendReply(reply_context, msg);
}

void PepperExternalFileRefBackend::GetMetadataComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  reply_context.params.set_result(ppapi::PlatformFileErrorToPepperError(error));

  PP_FileInfo pp_file_info;
  if (error == base::PLATFORM_FILE_OK) {
    ppapi::PlatformFileInfoToPepperFileInfo(
        file_info, PP_FILESYSTEMTYPE_EXTERNAL, &pp_file_info);
  } else {
    memset(&pp_file_info, 0, sizeof(pp_file_info));
  }

  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileRef_QueryReply(pp_file_info));
}

}  // namespace content
