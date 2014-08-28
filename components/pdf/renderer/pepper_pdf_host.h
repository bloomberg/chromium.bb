// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
#define COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/serialized_structs.h"

struct PP_ImageDataDesc;
struct PP_Size;
class SkBitmap;

namespace content {
class RendererPpapiHost;
}

namespace ppapi {
class HostResource;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}
}

namespace pdf {

class PepperPDFHost : public ppapi::host::ResourceHost {
 public:
  PepperPDFHost(content::RendererPpapiHost* host,
                PP_Instance instance,
                PP_Resource resource);
  virtual ~PepperPDFHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnHostMsgGetLocalizedString(ppapi::host::HostMessageContext* context,
                                      PP_ResourceString string_id);
  int32_t OnHostMsgDidStartLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgDidStopLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetContentRestriction(
      ppapi::host::HostMessageContext* context,
      int restrictions);
  int32_t OnHostMsgUserMetricsRecordAction(
      ppapi::host::HostMessageContext* context,
      const std::string& action);
  int32_t OnHostMsgHasUnsupportedFeature(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgPrint(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSaveAs(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGetResourceImage(ppapi::host::HostMessageContext* context,
                                    PP_ResourceImage image_id,
                                    float scale);
  int32_t OnHostMsgSetSelectedText(ppapi::host::HostMessageContext* context,
                                   const base::string16& selected_text);
  int32_t OnHostMsgSetLinkUnderCursor(ppapi::host::HostMessageContext* context,
                                      const std::string& url);

  bool CreateImageData(PP_Instance instance,
                       PP_ImageDataFormat format,
                       const PP_Size& size,
                       const SkBitmap& pixels_to_write,
                       ppapi::HostResource* result,
                       PP_ImageDataDesc* out_image_data_desc,
                       IPC::PlatformFileForTransit* out_image_handle,
                       uint32_t* out_byte_count);

  content::RendererPpapiHost* host_;

  DISALLOW_COPY_AND_ASSIGN(PepperPDFHost);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
