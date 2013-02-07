// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/file_type_conversion.h"

namespace content {

namespace {
// Used to check if the renderer has permission for the requested operation.
// TODO(viettrungluu): Verify these. They don't necessarily quite make sense,
// but it seems to be approximately what the file system code does.
const int kReadPermissions = base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_READ |
                             base::PLATFORM_FILE_EXCLUSIVE_READ;
const int kWritePermissions = base::PLATFORM_FILE_OPEN |
                              base::PLATFORM_FILE_CREATE |
                              base::PLATFORM_FILE_CREATE_ALWAYS |
                              base::PLATFORM_FILE_OPEN_TRUNCATED |
                              base::PLATFORM_FILE_WRITE |
                              base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                              base::PLATFORM_FILE_WRITE_ATTRIBUTES;
}  // namespace

PepperFlashFileMessageFilter::PepperFlashFileMessageFilter(
    PP_Instance instance,
    BrowserPpapiHost* host)
    : plugin_process_handle_(host->GetPluginProcessHandle()) {
  int unused;
  host->GetRenderViewIDsForInstance(instance, &render_process_id_, &unused);
  base::FilePath profile_data_directory = host->GetProfileDataDirectory();
  std::string plugin_name = host->GetPluginName();

  if (profile_data_directory.empty() || plugin_name.empty()) {
    // These are used to construct the path. If they are not set it means we
    // will construct a bad path and could provide access to the wrong files.
    // In this case, |plugin_data_directory_| will remain unset and
    // |ValidateAndConvertPepperFilePath| will fail.
    NOTREACHED();
  } else {
    plugin_data_directory_ = GetDataDirName(profile_data_directory).Append(
        base::FilePath::FromUTF8Unsafe(plugin_name));
  }
}

PepperFlashFileMessageFilter::~PepperFlashFileMessageFilter() {
}

// static
base::FilePath PepperFlashFileMessageFilter::GetDataDirName(
    const base::FilePath& profile_path) {
  return profile_path.Append(kPepperDataDirname);
}

scoped_refptr<base::TaskRunner>
PepperFlashFileMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  // The blocking pool provides a pool of threads to run file
  // operations, instead of a single thread which might require
  // queuing time.  Since these messages are synchronous as sent from
  // the plugin, the sending thread cannot send a new message until
  // this one returns, so there is no need to sequence tasks here.  If
  // the plugin has multiple threads, it cannot make assumptions about
  // ordering of IPC message sends, so it cannot make assumptions
  // about ordering of operations caused by those IPC messages.
  return scoped_refptr<base::TaskRunner>(BrowserThread::GetBlockingPool());
}

int32_t PepperFlashFileMessageFilter::OnResourceMessageReceived(
   const IPC::Message& msg,
   ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashFileMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_OpenFile,
                                      OnOpenFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_RenameFile,
                                      OnRenameFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_DeleteFileOrDir,
                                      OnDeleteFileOrDir)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_CreateDir,
                                      OnCreateDir)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_QueryFile,
                                      OnQueryFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_GetDirContents,
                                      OnGetDirContents)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_FlashFile_CreateTemporaryFile,
        OnCreateTemporaryFile)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashFileMessageFilter::OnOpenFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    int flags) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(path, flags);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file_handle = base::CreatePlatformFile(
      full_path, flags, NULL, &error);
  if (error != base::PLATFORM_FILE_OK) {
    DCHECK_EQ(file_handle, base::kInvalidPlatformFileValue);
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  // Make sure we didn't try to open a directory: directory fd shouldn't be
  // passed to untrusted processes because they open security holes.
  base::PlatformFileInfo info;
  if (!base::GetPlatformFileInfo(file_handle, &info) || info.is_directory) {
    // When in doubt, throw it out.
    base::ClosePlatformFile(file_handle);
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  IPC::PlatformFileForTransit file = IPC::GetFileHandleForProcess(file_handle,
      plugin_process_handle_, true);
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(ppapi::proxy::SerializedHandle(
      ppapi::proxy::SerializedHandle::FILE, file));
  SendReply(reply_context, IPC::Message());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFlashFileMessageFilter::OnRenameFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& from_path,
    const ppapi::PepperFilePath& to_path) {
  base::FilePath from_full_path = ValidateAndConvertPepperFilePath(
      from_path, kWritePermissions);
  base::FilePath to_full_path = ValidateAndConvertPepperFilePath(
      to_path, kWritePermissions);
  if (from_full_path.empty() || to_full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::Move(from_full_path, to_full_path);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnDeleteFileOrDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    bool recursive) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(
      path, kWritePermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::Delete(full_path, recursive);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}
int32_t PepperFlashFileMessageFilter::OnCreateDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(
      path, kWritePermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::CreateDirectory(full_path);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnQueryFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(
      path, kReadPermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  base::PlatformFileInfo info;
  bool result = file_util::GetFileInfo(full_path, &info);
  context->reply_msg = PpapiPluginMsg_FlashFile_QueryFileReply(info);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnGetDirContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(
      path, kReadPermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  ppapi::DirContents contents;
  file_util::FileEnumerator enumerator(full_path, false,
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES |
      file_util::FileEnumerator::INCLUDE_DOT_DOT);

  while (!enumerator.Next().empty()) {
    file_util::FileEnumerator::FindInfo info;
    enumerator.GetFindInfo(&info);
    ppapi::DirEntry entry = {
      file_util::FileEnumerator::GetFilename(info),
      file_util::FileEnumerator::IsDirectory(info)
    };
    contents.push_back(entry);
  }

  context->reply_msg = PpapiPluginMsg_FlashFile_GetDirContentsReply(contents);
  return PP_OK;
}

int32_t PepperFlashFileMessageFilter::OnCreateTemporaryFile(
    ppapi::host::HostMessageContext* context) {
  ppapi::PepperFilePath dir_path(
      ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL, base::FilePath());
  base::FilePath validated_dir_path = ValidateAndConvertPepperFilePath(
      dir_path, kReadPermissions | kWritePermissions);
  if (validated_dir_path.empty() ||
      (!file_util::DirectoryExists(validated_dir_path) &&
       !file_util::CreateDirectory(validated_dir_path))) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  base::FilePath file_path;
  if (!file_util::CreateTemporaryFileInDir(validated_dir_path, &file_path)) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_FAILED);
  }

  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file_handle = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_TEMPORARY |
      base::PLATFORM_FILE_DELETE_ON_CLOSE,
      NULL, &error);

  if (error != base::PLATFORM_FILE_OK) {
    DCHECK_EQ(file_handle, base::kInvalidPlatformFileValue);
    return ppapi::PlatformFileErrorToPepperError(error);
  }

  IPC::PlatformFileForTransit file = IPC::GetFileHandleForProcess(file_handle,
      plugin_process_handle_, true);
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(ppapi::proxy::SerializedHandle(
      ppapi::proxy::SerializedHandle::FILE, file));
  SendReply(reply_context, IPC::Message());
  return PP_OK_COMPLETIONPENDING;
}

base::FilePath PepperFlashFileMessageFilter::ValidateAndConvertPepperFilePath(
    const ppapi::PepperFilePath& pepper_path,
    int flags) {
  base::FilePath file_path;  // Empty path returned on error.
  switch (pepper_path.domain()) {
    case ppapi::PepperFilePath::DOMAIN_ABSOLUTE:
      if (pepper_path.path().IsAbsolute() &&
          ChildProcessSecurityPolicyImpl::GetInstance()->HasPermissionsForFile(
              render_process_id_, pepper_path.path(), flags))
        file_path = pepper_path.path();
      break;
    case ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL:
      // This filter provides the module name portion of the path to prevent
      // plugins from accessing each other's data.
      if (!plugin_data_directory_.empty() &&
          !pepper_path.path().IsAbsolute() &&
          !pepper_path.path().ReferencesParent())
        file_path = plugin_data_directory_.Append(pepper_path.path());
      break;
    default:
      NOTREACHED();
      break;
  }
  return file_path;
}

}  // namespace content
