// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FLASH_CLIPBOARD_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FLASH_CLIPBOARD_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/flash_clipboard_format_registry.h"

namespace webkit_glue {
class ClipboardClient;
class ScopedClipboardWriterGlue;
}

namespace content {

class RendererPpapiHost;

// The host resource for accessing the clipboard in Pepper. Pepper supports
// reading/writing custom formats from the clipboard. Currently, all custom
// formats that are read/written from the clipboard through pepper are stored
// in a single real clipboard format (in the same way the "web custom" clipboard
// formats are). This is done so that we don't have to have use real clipboard
// types for each custom clipboard format which may be a limited resource on
// a particular platform.

// TODO(raymes): This host can be moved to the browser process and we would
// avoid crossing an additional process boundary to access the clipboard from
// pepper. Moving the host code to the browser is straightforward but it also
// means we have to deal with handling the clipboard accesses on the clipboard
// thread.
class PepperFlashClipboardHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashClipboardHost(RendererPpapiHost* host,
                           PP_Instance instance,
                           PP_Resource resource);
  virtual ~PepperFlashClipboardHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnRegisterCustomFormat(
      ppapi::host::HostMessageContext* host_context,
      const std::string& format_name);
  int32_t OnIsFormatAvailable(
      ppapi::host::HostMessageContext* host_context,
      uint32_t clipboard_type,
      uint32_t format);
  int32_t OnReadData(
      ppapi::host::HostMessageContext* host_context,
      uint32_t clipoard_type,
      uint32_t format);
  int32_t OnWriteData(
      ppapi::host::HostMessageContext* host_context,
      uint32_t clipboard_type,
      const std::vector<uint32_t>& formats,
      const std::vector<std::string>& data);

  int32_t WriteClipboardDataItem(
      uint32_t format,
      const std::string& data,
      webkit_glue::ScopedClipboardWriterGlue* scw);

  scoped_ptr<webkit_glue::ClipboardClient> clipboard_client_;
  ppapi::FlashClipboardFormatRegistry custom_formats_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashClipboardHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FLASH_CLIPBOARD_HOST_H_
