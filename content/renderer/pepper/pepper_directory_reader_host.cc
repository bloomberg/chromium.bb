// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_directory_reader_host.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/ppb_file_ref_shared.h"
#include "ppapi/thunk/enter.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_FileRef_API;
using webkit::ppapi::PPB_FileRef_Impl;

namespace content {

namespace {

std::string FilePathStringToUTF8String(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return WideToUTF8(str);
#elif defined(OS_POSIX)
  return str;
#else
#error "Unsupported platform."
#endif
}

base::FilePath::StringType UTF8StringToFilePathString(const std::string& str) {
#if defined(OS_WIN)
  return UTF8ToWide(str);
#elif defined(OS_POSIX)
  return str;
#else
#error "Unsupported platform."
#endif
}

class ReadDirectoryCallback : public fileapi::FileSystemCallbackDispatcher {
 public:
  typedef base::Callback<void (const PepperDirectoryReaderHost::Entries&,
                               bool, int32_t)>
      OnReadDirectoryCallback;

  explicit ReadDirectoryCallback(const OnReadDirectoryCallback& callback)
      : callback_(callback) {}
  virtual ~ReadDirectoryCallback() {}

  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info,
                               const base::FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidCreateSnapshotFile(
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    callback_.Run(entries, has_more, PP_OK);
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error) OVERRIDE {
    callback_.Run(PepperDirectoryReaderHost::Entries(),
                  false,
                  ppapi::PlatformFileErrorToPepperError(error));
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFile(base::PlatformFile file) OVERRIDE {
    NOTREACHED();
  }

 private:
  OnReadDirectoryCallback callback_;
};

}  // namespace

PepperDirectoryReaderHost::PepperDirectoryReaderHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PepperDirectoryReaderHost::~PepperDirectoryReaderHost() {
}

int32_t PepperDirectoryReaderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperDirectoryReaderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_DirectoryReader_GetEntries, OnGetEntries)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperDirectoryReaderHost::OnGetEntries(
    ppapi::host::HostMessageContext* host_context,
    const ppapi::HostResource& resource) {
  reply_context_ = host_context->MakeReplyMessageContext();

  EnterResource<PPB_FileRef_API> enter(resource.host_resource(), true);
  if (enter.failed())
    return PP_ERROR_FAILED;
  directory_ref_ = static_cast<PPB_FileRef_Impl*>(enter.object());

  if (directory_ref_->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;

  webkit::ppapi::PluginInstance* plugin_instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  if (!plugin_instance->delegate()->ReadDirectory(
          directory_ref_->GetFileSystemURL(),
          new ReadDirectoryCallback(
              base::Bind(&PepperDirectoryReaderHost::OnReadDirectory,
                         weak_factory_.GetWeakPtr()))))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

void PepperDirectoryReaderHost::OnReadDirectory(const Entries& entries,
                                                bool has_more,
                                                int32_t result) {
  // The current filesystem backend always returns false.
  DCHECK(!has_more);
  if (result == PP_OK && !AddNewEntries(entries))
    result = PP_ERROR_FAILED;
  SendGetEntriesReply(result);
}

bool PepperDirectoryReaderHost::AddNewEntries(const Entries& entries) {
  std::string dir_path = directory_ref_->GetCreateInfo().path;
  if (dir_path[dir_path.size() - 1] != '/')
    dir_path += '/';
  base::FilePath::StringType dir_file_path =
      UTF8StringToFilePathString(dir_path);

  for (Entries::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    EntryData data;
    data.file_ref = PPB_FileRef_Impl::CreateInternal(
        directory_ref_->file_system()->pp_resource(),
        FilePathStringToUTF8String(dir_file_path + it->name));
    if (!data.file_ref) {
      entry_data_.clear();
      return false;
    }
    data.file_type = it->is_directory ?
                     PP_FILETYPE_DIRECTORY : PP_FILETYPE_REGULAR;
    entry_data_.push_back(data);
  }

  return true;
}

void PepperDirectoryReaderHost::SendGetEntriesReply(int32_t result) {
  std::vector<ppapi::PPB_FileRef_CreateInfo> host_resources;
  std::vector<PP_FileType> file_types;

  for (std::vector<EntryData>::iterator it = entry_data_.begin();
       it != entry_data_.end(); ++it) {
    // Add a ref count on behalf of the plugin side.
    it->file_ref->GetReference();
    host_resources.push_back(it->file_ref->GetCreateInfo());
    file_types.push_back(it->file_type);
  }
  entry_data_.clear();

  reply_context_.params.set_result(result);
  host()->SendReply(
      reply_context_,
      PpapiPluginMsg_DirectoryReader_GetEntriesReply(host_resources,
                                                     file_types));
}

PepperDirectoryReaderHost::EntryData::EntryData() {
}

PepperDirectoryReaderHost::EntryData::~EntryData() {
}

}  // namespace content
