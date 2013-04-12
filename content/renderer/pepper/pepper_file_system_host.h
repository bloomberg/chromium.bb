// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class PepperFileSystemHost :
    public ppapi::host::ResourceHost,
    public base::SupportsWeakPtr<PepperFileSystemHost> {
 public:
  PepperFileSystemHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource,
                       PP_FileSystemType type);
  virtual ~PepperFileSystemHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // Supports FileRefs direct access on the host side.
  PP_FileSystemType GetType() const { return type_; }
  bool IsOpened() const { return opened_; }
  GURL GetRootUrl() const { return root_url_; }

  // It's public only to allow PlatformCallbackAdaptor to access.
  void OpenFileSystemReply(int32_t pp_error,
                           const GURL& root);

 private:
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        int64_t expected_size);

  RendererPpapiHost* renderer_ppapi_host_;
  ppapi::host::ReplyMessageContext reply_context_;
  base::WeakPtrFactory<PepperFileSystemHost> weak_factory_;

  PP_FileSystemType type_;
  bool opened_;  // whether open is successful.
  GURL root_url_;
  bool called_open_;  // whether open has been called.

  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_
