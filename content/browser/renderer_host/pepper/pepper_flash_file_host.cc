// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_flash_file_host.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/task_runner.h"
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

// All file messages are handled by BrowserThread's blocking pool.
class FileMessageFilter : public ppapi::host::ResourceMessageFilter {
 public:
  FileMessageFilter(const std::string& plugin_name,
                    const FilePath& profile_data_directory,
                    int plugin_process_id,
                    base::ProcessHandle plugin_process_handle);
 protected:
  // ppapi::host::ResourceMessageFilter implementation.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  virtual ~FileMessageFilter();

  int32_t OnOpenFile(ppapi::host::HostMessageContext* context,
                     const ppapi::PepperFilePath& path,
                     int flags);
  int32_t OnRenameFile(ppapi::host::HostMessageContext* context,
                       const ppapi::PepperFilePath& from_path,
                       const ppapi::PepperFilePath& to_path);
  int32_t OnDeleteFileOrDir(ppapi::host::HostMessageContext* context,
                            const ppapi::PepperFilePath& path,
                            bool recursive);
  int32_t OnCreateDir(ppapi::host::HostMessageContext* context,
                      const ppapi::PepperFilePath& path);
  int32_t OnQueryFile(ppapi::host::HostMessageContext* context,
                      const ppapi::PepperFilePath& path);
  int32_t OnGetDirContents(ppapi::host::HostMessageContext* context,
                           const ppapi::PepperFilePath& path);
  int32_t OnCreateTemporaryFile(ppapi::host::HostMessageContext* context);

  FilePath ValidateAndConvertPepperFilePath(
      const ppapi::PepperFilePath& pepper_path,
      int flags);

  FilePath plugin_data_directory_;
  int plugin_process_id_;
  base::ProcessHandle plugin_process_handle_;
};

FileMessageFilter::FileMessageFilter(
    const std::string& plugin_name,
    const FilePath& profile_data_directory,
    int plugin_process_id,
    base::ProcessHandle plugin_process_handle)
    : plugin_process_id_(plugin_process_id),
      plugin_process_handle_(plugin_process_handle) {
  if (profile_data_directory.empty() || plugin_name.empty()) {
    // These are used to construct the path. If they are not set it means we
    // will construct a bad path and could provide access to the wrong files.
    // In this case, |plugin_data_directory_| will remain unset and
    // |ValidateAndConvertPepperFilePath| will fail.
    NOTREACHED();
  } else {
    plugin_data_directory_ = PepperFlashFileHost::GetDataDirName(
        profile_data_directory).Append(FilePath::FromUTF8Unsafe(plugin_name));
  }
}

FileMessageFilter::~FileMessageFilter() {
}

scoped_refptr<base::TaskRunner>
FileMessageFilter::OverrideTaskRunnerForMessage(const IPC::Message& msg) {
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

int32_t FileMessageFilter::OnResourceMessageReceived(
   const IPC::Message& msg,
   ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(FileMessageFilter, msg)
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

int32_t FileMessageFilter::OnOpenFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    int flags) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path, flags);
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

int32_t FileMessageFilter::OnRenameFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& from_path,
    const ppapi::PepperFilePath& to_path) {
  FilePath from_full_path = ValidateAndConvertPepperFilePath(from_path,
                                                             kWritePermissions);
  FilePath to_full_path = ValidateAndConvertPepperFilePath(to_path,
                                                           kWritePermissions);
  if (from_full_path.empty() || to_full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::Move(from_full_path, to_full_path);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}

int32_t FileMessageFilter::OnDeleteFileOrDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    bool recursive) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path,
                                                        kWritePermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::Delete(full_path, recursive);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}
int32_t FileMessageFilter::OnCreateDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path,
                                                        kWritePermissions);
  if (full_path.empty()) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  bool result = file_util::CreateDirectory(full_path);
  return ppapi::PlatformFileErrorToPepperError(result ?
      base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
}

int32_t FileMessageFilter::OnQueryFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path,
                                                        kReadPermissions);
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

int32_t FileMessageFilter::OnGetDirContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path, kReadPermissions);
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

int32_t FileMessageFilter::OnCreateTemporaryFile(
    ppapi::host::HostMessageContext* context) {
  ppapi::PepperFilePath dir_path(
      ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL, FilePath());
  FilePath validated_dir_path = ValidateAndConvertPepperFilePath(
      dir_path, kReadPermissions | kWritePermissions);
  if (validated_dir_path.empty() ||
      (!file_util::DirectoryExists(validated_dir_path) &&
       !file_util::CreateDirectory(validated_dir_path))) {
    return ppapi::PlatformFileErrorToPepperError(
        base::PLATFORM_FILE_ERROR_ACCESS_DENIED);
  }

  FilePath file_path;
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

FilePath FileMessageFilter::ValidateAndConvertPepperFilePath(
    const ppapi::PepperFilePath& pepper_path,
    int flags) {
  FilePath file_path;  // Empty path returned on error.
  switch (pepper_path.domain()) {
    case ppapi::PepperFilePath::DOMAIN_ABSOLUTE:
      if (pepper_path.path().IsAbsolute() &&
          ChildProcessSecurityPolicyImpl::GetInstance()->HasPermissionsForFile(
              plugin_process_id_, pepper_path.path(), flags))
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

}  // namespace

PepperFlashFileHost::PepperFlashFileHost(
    BrowserPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
  AddFilter(scoped_refptr<ppapi::host::ResourceMessageFilter>(
      new FileMessageFilter(host->GetPluginName(),
                            host->GetProfileDataDirectory(),
                            host->GetPluginProcessID(),
                            host->GetPluginProcessHandle())));
}

PepperFlashFileHost::~PepperFlashFileHost() {
}

// static
FilePath PepperFlashFileHost::GetDataDirName(const FilePath& profile_path) {
  return profile_path.Append(kPepperDataDirname);
}

}  // namespace content
