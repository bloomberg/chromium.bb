// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

class RenderViewImpl;

namespace content {

class PepperInstanceStateAccessor;

class CONTENT_EXPORT PepperFileChooserHost
    : public ppapi::host::ResourceHost,
      public base::SupportsWeakPtr<PepperFileChooserHost> {
 public:
  // Structure to store the information about chosen files.
  struct ChosenFileInfo {
    ChosenFileInfo(const std::string& path, const std::string& display_name);
    std::string path;
    std::string display_name;  // May be empty.
  };

  PepperFileChooserHost(ppapi::host::PpapiHost* host,
                        PP_Instance instance,
                        PP_Resource resource,
                        RenderViewImpl* render_view,
                        PepperInstanceStateAccessor* state);
  virtual ~PepperFileChooserHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  void StoreChosenFiles(const std::vector<ChosenFileInfo>& files);

 private:
  class CompletionHandler;

  int32_t OnMsgShow(ppapi::host::HostMessageContext* context,
                    bool save_as,
                    bool open_multiple,
                    const std::string& suggested_file_name,
                    const std::vector<std::string>& accept_mime_types);

  // Non-owning pointers.
  RenderViewImpl* render_view_;
  PepperInstanceStateAccessor* instance_state_;

  ppapi::proxy::ResourceMessageReplyParams reply_params_;
  CompletionHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileChooserHost);
};

}  // namespace ppapi

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
