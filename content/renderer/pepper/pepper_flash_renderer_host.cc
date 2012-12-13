// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_flash_renderer_host.h"

#include <vector>

#include "content/common/view_messages.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/render_thread_impl.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkTemplates.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;

namespace content {

PepperFlashRendererHost::PepperFlashRendererHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host) {
}

PepperFlashRendererHost::~PepperFlashRendererHost() {
}

int32_t PepperFlashRendererHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashRendererHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_GetProxyForURL,
                                      OnMsgGetProxyForURL);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_SetInstanceAlwaysOnTop,
                                      OnMsgSetInstanceAlwaysOnTop);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_DrawGlyphs,
                                      OnMsgDrawGlyphs);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_Navigate,
                                      OnMsgNavigate);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_IsRectTopmost,
                                      OnMsgIsRectTopmost);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashRendererHost::OnMsgGetProxyForURL(
    ppapi::host::HostMessageContext* host_context,
    const std::string& url) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_ERROR_FAILED;
  bool result;
  std::string proxy;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_ResolveProxy(gurl, &result, &proxy));
  if (!result)
    return PP_ERROR_FAILED;
  host_context->reply_msg = PpapiPluginMsg_Flash_GetProxyForURLReply(proxy);
  return PP_OK;
}

int32_t PepperFlashRendererHost::OnMsgSetInstanceAlwaysOnTop(
    ppapi::host::HostMessageContext* host_context,
    bool on_top) {
  webkit::ppapi::PluginInstance* plugin_instance =
      host_->GetPluginInstance(pp_instance());
  if (plugin_instance)
    plugin_instance->set_always_on_top(on_top);
  // Since no reply is sent for this message, it doesn't make sense to return an
  // error.
  return PP_OK;
}

int32_t PepperFlashRendererHost::OnMsgDrawGlyphs(
    ppapi::host::HostMessageContext* host_context,
    ppapi::proxy::PPBFlash_DrawGlyphs_Params params) {
  if (params.glyph_indices.size() != params.glyph_advances.size() ||
      params.glyph_indices.empty())
    return PP_ERROR_FAILED;

  EnterResourceNoLock<PPB_ImageData_API> enter(
      params.image_data.host_resource(), true);
  if (enter.failed())
    return PP_ERROR_FAILED;
  webkit::ppapi::PPB_ImageData_Impl* image_resource =
      static_cast<webkit::ppapi::PPB_ImageData_Impl*>(enter.object());

  webkit::ppapi::ImageDataAutoMapper mapper(image_resource);
  if (!mapper.is_valid())
    return PP_ERROR_FAILED;

  // Set up the typeface.
  int style = SkTypeface::kNormal;
  if (static_cast<PP_BrowserFont_Trusted_Weight>(params.font_desc.weight) >=
      PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD)
    style |= SkTypeface::kBold;
  if (params.font_desc.italic)
    style |= SkTypeface::kItalic;
  skia::RefPtr<SkTypeface> typeface = skia::AdoptRef(
      SkTypeface::CreateFromName(params.font_desc.face.c_str(),
                                 static_cast<SkTypeface::Style>(style)));
  if (!typeface)
    return PP_ERROR_FAILED;

  // Set up the canvas.
  SkCanvas* canvas = image_resource->GetPlatformCanvas();
  SkAutoCanvasRestore acr(canvas, true);

  // Clip is applied in pixels before the transform.
  SkRect clip_rect = {
    SkIntToScalar(params.clip.point.x),
    SkIntToScalar(params.clip.point.y),
    SkIntToScalar(params.clip.point.x + params.clip.size.width),
    SkIntToScalar(params.clip.point.y + params.clip.size.height)
  };
  canvas->clipRect(clip_rect);

  // Convert & set the matrix.
  SkMatrix matrix;
  matrix.set(SkMatrix::kMScaleX, SkFloatToScalar(params.transformation[0][0]));
  matrix.set(SkMatrix::kMSkewX,  SkFloatToScalar(params.transformation[0][1]));
  matrix.set(SkMatrix::kMTransX, SkFloatToScalar(params.transformation[0][2]));
  matrix.set(SkMatrix::kMSkewY,  SkFloatToScalar(params.transformation[1][0]));
  matrix.set(SkMatrix::kMScaleY, SkFloatToScalar(params.transformation[1][1]));
  matrix.set(SkMatrix::kMTransY, SkFloatToScalar(params.transformation[1][2]));
  matrix.set(SkMatrix::kMPersp0, SkFloatToScalar(params.transformation[2][0]));
  matrix.set(SkMatrix::kMPersp1, SkFloatToScalar(params.transformation[2][1]));
  matrix.set(SkMatrix::kMPersp2, SkFloatToScalar(params.transformation[2][2]));
  canvas->concat(matrix);

  SkPaint paint;
  paint.setColor(params.color);
  paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint.setAntiAlias(true);
  paint.setHinting(SkPaint::kFull_Hinting);
  paint.setTextSize(SkIntToScalar(params.font_desc.size));
  paint.setTypeface(typeface.get());  // Takes a ref and manages lifetime.
  if (params.allow_subpixel_aa) {
    paint.setSubpixelText(true);
    paint.setLCDRenderText(true);
  }

  SkScalar x = SkIntToScalar(params.position.x);
  SkScalar y = SkIntToScalar(params.position.y);

  // Build up the skia advances.
  size_t glyph_count = params.glyph_indices.size();
  if (glyph_count == 0)
    return PP_OK;
  std::vector<SkPoint> storage;
  storage.resize(glyph_count);
  SkPoint* sk_positions = &storage[0];
  for (uint32_t i = 0; i < glyph_count; i++) {
    sk_positions[i].set(x, y);
    x += SkFloatToScalar(params.glyph_advances[i].x);
    y += SkFloatToScalar(params.glyph_advances[i].y);
  }

  canvas->drawPosText(&params.glyph_indices[0], glyph_count * 2, sk_positions,
                      paint);

  return PP_OK;
}

int32_t PepperFlashRendererHost::OnMsgNavigate(
    ppapi::host::HostMessageContext* host_context,
    const ppapi::URLRequestInfoData& data,
    const std::string& target,
    bool from_user_action) {
  webkit::ppapi::PluginInstance* plugin_instance =
      host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  // We need to allow re-entrancy here, because this may call into Javascript
  // (e.g. with a "javascript:" URL), or do things like navigate away from the
  // page, either one of which will need to re-enter into the plugin.
  // It is safe, because it is essentially equivalent to NPN_GetURL, where Flash
  // would expect re-entrancy.
  ppapi::proxy::HostDispatcher* host_dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
  host_dispatcher->set_allow_plugin_reentrancy();

  plugin_instance->Navigate(data, target.c_str(), from_user_action);
  return PP_OK;
}

int32_t PepperFlashRendererHost::OnMsgIsRectTopmost(
    ppapi::host::HostMessageContext* host_context,
    const PP_Rect& rect) {
  webkit::ppapi::PluginInstance* plugin_instance =
      host_->GetPluginInstance(pp_instance());
  if (plugin_instance && plugin_instance->IsRectTopmost(
      gfx::Rect(rect.point.x, rect.point.y,rect.size.width, rect.size.height)))
    return PP_OK;
  return PP_ERROR_FAILED;
}

}  // namespace content
