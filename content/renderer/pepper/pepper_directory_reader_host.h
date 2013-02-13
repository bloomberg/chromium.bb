// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_DIRECTORY_READER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_DIRECTORY_READER_HOST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

namespace ppapi {
struct PPB_FileRef_CreateInfo;
}

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperDirectoryReaderHost
    : public ppapi::host::ResourceHost {
 public:
  typedef std::vector<base::FileUtilProxy::Entry> Entries;

  PepperDirectoryReaderHost(RendererPpapiHost* host,
                            PP_Instance instance,
                            PP_Resource resource);
  virtual ~PepperDirectoryReaderHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  struct EntryData {
    EntryData();
    ~EntryData();
    scoped_refptr<webkit::ppapi::PPB_FileRef_Impl> file_ref;
    PP_FileType file_type;
  };

  int32_t OnGetEntries(ppapi::host::HostMessageContext* host_context,
                       const ppapi::HostResource& resource);
  void OnReadDirectory(const Entries& entries, bool has_more, int32_t result);
  bool AddNewEntries(const Entries& entries);

  void SendGetEntriesReply(int32_t result);

  RendererPpapiHost* renderer_ppapi_host_;
  ppapi::host::ReplyMessageContext reply_context_;

  scoped_refptr<webkit::ppapi::PPB_FileRef_Impl> directory_ref_;

  // Ensures that the resources are alive for as long as the host is.
  std::vector<EntryData> entry_data_;

  base::WeakPtrFactory<PepperDirectoryReaderHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperDirectoryReaderHost);
};

}

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_DIRECTORY_READER_HOST_H_
