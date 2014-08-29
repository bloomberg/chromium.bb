// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_renderer_pdf_helper.h"

#include "components/pdf/renderer/pepper_pdf_host.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace athena {

namespace {

class PDFRendererHostFactory : public ppapi::host::HostFactory {
 public:
  explicit PDFRendererHostFactory(content::RendererPpapiHost* host)
      : host_(host) {}
  virtual ~PDFRendererHostFactory() {}

 private:
  // ppapi::host::HostFactory:
  virtual scoped_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      const ppapi::proxy::ResourceMessageCallParams& params,
      PP_Instance instance,
      const IPC::Message& message) OVERRIDE {
    DCHECK_EQ(host_->GetPpapiHost(), host);
    // Make sure the plugin is giving us a valid instance for this resource.
    if (!host_->IsValidInstance(instance))
      return scoped_ptr<ppapi::host::ResourceHost>();

    if (host_->GetPpapiHost()->permissions().HasPermission(
            ppapi::PERMISSION_PRIVATE)) {
      switch (message.type()) {
        case PpapiHostMsg_PDF_Create::ID:
          return scoped_ptr<ppapi::host::ResourceHost>(
              new pdf::PepperPDFHost(host_, instance, params.pp_resource()));

        case PpapiHostMsg_FlashFontFile_Create::ID:
          return scoped_ptr<ppapi::host::ResourceHost>(
              new ppapi::host::ResourceHost(host_->GetPpapiHost(),
                                            instance,
                                            params.pp_resource()));
      }
    }

    return scoped_ptr<ppapi::host::ResourceHost>();
  }

  // Not owned by this object.
  content::RendererPpapiHost* host_;

  DISALLOW_COPY_AND_ASSIGN(PDFRendererHostFactory);
};

}  // namespace

AthenaRendererPDFHelper::AthenaRendererPDFHelper(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

AthenaRendererPDFHelper::~AthenaRendererPDFHelper() {}

void AthenaRendererPDFHelper::DidCreatePepperPlugin(
    content::RendererPpapiHost* host) {
  host->GetPpapiHost()->AddHostFactoryFilter(
      scoped_ptr<ppapi::host::HostFactory>(new PDFRendererHostFactory(host)));
}

}  // namespace athena
