// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace content {

class BrowserPpapiHost;

class PepperFileSystemBrowserHost :
    public ppapi::host::ResourceHost,
    public base::SupportsWeakPtr<PepperFileSystemBrowserHost> {
 public:
  PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                              PP_Instance instance,
                              PP_Resource resource,
                              PP_FileSystemType type);
  virtual ~PepperFileSystemBrowserHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;
  virtual bool IsFileSystemHost() OVERRIDE;

  // Supports FileRefs direct access on the host side.
  PP_FileSystemType GetType() const { return type_; }
  bool IsOpened() const { return opened_; }
  GURL GetRootUrl() const { return root_url_; }
  scoped_refptr<fileapi::FileSystemContext> GetFileSystemContext() const {
    return fs_context_;
  }

 private:
  void GotFileSystemContext(
      ppapi::host::ReplyMessageContext reply_context,
      fileapi::FileSystemType file_system_type,
      scoped_refptr<fileapi::FileSystemContext> fs_context);
  void OpenFileSystemComplete(
      ppapi::host::ReplyMessageContext reply_context,
      base::PlatformFileError error,
      const std::string& name,
      const GURL& root);

  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        int64_t expected_size);
  int32_t OnHostMsgInitIsolatedFileSystem(
      ppapi::host::HostMessageContext* context,
      const std::string& fsid);

  BrowserPpapiHost* browser_ppapi_host_;
  base::WeakPtrFactory<PepperFileSystemBrowserHost> weak_factory_;

  PP_FileSystemType type_;
  bool opened_;  // whether open is successful.
  GURL root_url_;
  scoped_refptr<fileapi::FileSystemContext> fs_context_;
  bool called_open_;  // whether open has been called.

  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemBrowserHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_
