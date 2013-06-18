// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace content {

// Internal and external filesystems have very different codepaths for
// performing FileRef operations. The logic is split into separate classes
// to make it easier to read.
class PepperFileRefBackend {
 public:
  virtual ~PepperFileRefBackend();

  virtual int32_t MakeDirectory(ppapi::host::ReplyMessageContext context,
                                bool make_ancestors) = 0;
  virtual int32_t Touch(ppapi::host::ReplyMessageContext context,
                        PP_Time last_accessed_time,
                        PP_Time last_modified_time) = 0;
  virtual int32_t Delete(ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t Rename(ppapi::host::ReplyMessageContext context,
                         PP_Resource new_file_ref) = 0;
  virtual int32_t Query(ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t ReadDirectoryEntries(
      ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t GetAbsolutePath(
      ppapi::host::ReplyMessageContext context) = 0;

  virtual fileapi::FileSystemURL GetFileSystemURL() const = 0;
};

class CONTENT_EXPORT PepperFileRefHost
    : public ppapi::host::ResourceHost,
      public base::SupportsWeakPtr<PepperFileRefHost> {
 public:
  PepperFileRefHost(BrowserPpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource,
                    PP_Resource file_system,
                    const std::string& internal_path);

  // TODO(teravest): Add a constructor for external paths.

  virtual ~PepperFileRefHost();

  // ResourceHost overrides.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;
  virtual PepperFileRefHost* AsPepperFileRefHost() OVERRIDE;

  // Required to support Rename().
  PP_FileSystemType GetFileSystemType() const;
  fileapi::FileSystemURL GetFileSystemURL() const;

 private:
  int32_t OnMakeDirectory(ppapi::host::HostMessageContext* context,
                          bool make_ancestors);
  int32_t OnTouch(ppapi::host::HostMessageContext* context,
                  PP_Time last_access_time,
                  PP_Time last_modified_time);
  int32_t OnDelete(ppapi::host::HostMessageContext* context);
  int32_t OnRename(ppapi::host::HostMessageContext* context,
                   PP_Resource new_file_ref);
  int32_t OnQuery(ppapi::host::HostMessageContext* context);
  int32_t OnReadDirectoryEntries(ppapi::host::HostMessageContext* context);
  int32_t OnGetAbsolutePath(ppapi::host::HostMessageContext* context);

  BrowserPpapiHost* host_;
  scoped_ptr<PepperFileRefBackend> backend_;
  PP_FileSystemType fs_type_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileRefHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_
