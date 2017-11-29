// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pepper_pdf_host.h"

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"

namespace pdf {

namespace {

// --single-process model may fail in CHECK(!g_print_client) if there exist
// more than two RenderThreads, so here we use TLS for g_print_client.
// See http://crbug.com/457580.
base::LazyInstance<base::ThreadLocalPointer<PepperPDFHost::PrintClient>>::Leaky
    g_print_client_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

PepperPDFHost::PepperPDFHost(content::RendererPpapiHost* host,
                             PP_Instance instance,
                             PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      binding_(this) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return;

  mojom::PdfListenerPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));
  service->SetListener(std::move(listener));
}

PepperPDFHost::~PepperPDFHost() {}

// static
bool PepperPDFHost::InvokePrintingForInstance(PP_Instance instance_id) {
  return g_print_client_tls.Pointer()->Get()
             ? g_print_client_tls.Pointer()->Get()->Print(instance_id)
             : false;
}

// static
void PepperPDFHost::SetPrintClient(PepperPDFHost::PrintClient* client) {
  CHECK(!g_print_client_tls.Pointer()->Get())
      << "There should only be a single PrintClient for one RenderThread.";
  g_print_client_tls.Pointer()->Set(client);
}

int32_t PepperPDFHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperPDFHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStartLoading,
                                        OnHostMsgDidStartLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStopLoading,
                                        OnHostMsgDidStopLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_UserMetricsRecordAction,
                                      OnHostMsgUserMetricsRecordAction)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_HasUnsupportedFeature,
                                        OnHostMsgHasUnsupportedFeature)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_Print, OnHostMsgPrint)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_SaveAs,
                                        OnHostMsgSaveAs)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetSelectedText,
                                      OnHostMsgSetSelectedText)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetLinkUnderCursor,
                                      OnHostMsgSetLinkUnderCursor)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetContentRestriction,
                                      OnHostMsgSetContentRestriction)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityViewportInfo,
        OnHostMsgSetAccessibilityViewportInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityDocInfo,
        OnHostMsgSetAccessibilityDocInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityPageInfo,
        OnHostMsgSetAccessibilityPageInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SelectionChanged,
                                      OnHostMsgSelectionChanged)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_DidScroll,
                                      OnHostMsgDidScroll)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgDidStartLoading(
    ppapi::host::HostMessageContext* context) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return PP_ERROR_FAILED;

  render_frame->PluginDidStartLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStopLoading(
    ppapi::host::HostMessageContext* context) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return PP_ERROR_FAILED;

  render_frame->PluginDidStopLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetContentRestriction(
    ppapi::host::HostMessageContext* context,
    int restrictions) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->UpdateContentRestrictions(restrictions);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgUserMetricsRecordAction(
    ppapi::host::HostMessageContext* context,
    const std::string& action) {
  if (action.empty())
    return PP_ERROR_FAILED;
  content::RenderThread::Get()->RecordComputedAction(action);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgHasUnsupportedFeature(
    ppapi::host::HostMessageContext* context) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->HasUnsupportedFeature();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgPrint(
    ppapi::host::HostMessageContext* context) {
  return InvokePrintingForInstance(pp_instance()) ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgSaveAs(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  GURL url = instance->GetPluginURL();
  content::Referrer referrer;
  referrer.url = url;
  referrer.policy = blink::kWebReferrerPolicyDefault;
  referrer = content::Referrer::SanitizeForRequest(url, referrer);

  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->SaveUrlAs(url, referrer);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetSelectedText(
    ppapi::host::HostMessageContext* context,
    const base::string16& selected_text) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetSelectedText(selected_text);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetLinkUnderCursor(
    ppapi::host::HostMessageContext* context,
    const std::string& url) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetLinkUnderCursor(url);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetAccessibilityViewportInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityViewportInfo& viewport_info) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetAccessibilityDocInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityDocInfo& doc_info) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetAccessibilityPageInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityPageInfo& page_info,
    const std::vector<PP_PrivateAccessibilityTextRunInfo>& text_run_info,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  pdf_accessibility_tree_->SetAccessibilityPageInfo(
      page_info, text_run_info, chars);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSelectionChanged(
    ppapi::host::HostMessageContext* context,
    const PP_FloatPoint& left,
    int32_t left_height,
    const PP_FloatPoint& right,
    int32_t right_height) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->SelectionChanged(gfx::PointF(left.x, left.y), left_height,
                            gfx::PointF(right.x, right.y), right_height);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidScroll(
    ppapi::host::HostMessageContext* context) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->DidScroll();
  return PP_OK;
}

void PepperPDFHost::CreatePdfAccessibilityTreeIfNeeded() {
  if (!pdf_accessibility_tree_) {
    pdf_accessibility_tree_ =
        base::MakeUnique<PdfAccessibilityTree>(host_, pp_instance());
  }
}

content::RenderFrame* PepperPDFHost::GetRenderFrame() {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  return instance ? instance->GetRenderFrame() : nullptr;
}

mojom::PdfService* PepperPDFHost::GetRemotePdfService() {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return nullptr;

  if (!remote_pdf_service_) {
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        &remote_pdf_service_);
  }
  return remote_pdf_service_.get();
}

void PepperPDFHost::SetCaretPosition(const gfx::PointF& position) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->SetCaretPosition(position);
}

void PepperPDFHost::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->MoveRangeSelectionExtent(extent);
}

void PepperPDFHost::SetSelectionBounds(const gfx::PointF& base,
                                       const gfx::PointF& extent) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->SetSelectionBounds(base, extent);
}

}  // namespace pdf
