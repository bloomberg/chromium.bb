// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_graphics_3d_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_platform_context_3d.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_view_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "webkit/common/webpreferences.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using blink::WebConsoleMessage;
using blink::WebFrame;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

}  // namespace.

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PP_Instance instance)
    : PPB_Graphics3D_Shared(instance),
      bound_to_instance_(false),
      commit_pending_(false),
      weak_ptr_factory_(this) {}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() { DestroyGLES2Impl(); }

// static
PP_Resource PPB_Graphics3D_Impl::Create(PP_Instance instance,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->Init(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

// static
PP_Resource PPB_Graphics3D_Impl::CreateRaw(PP_Instance instance,
                                           PP_Resource share_context,
                                           const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

PP_Bool PPB_Graphics3D_Impl::SetGetBuffer(int32_t transfer_buffer_id) {
  GetCommandBuffer()->SetGetBuffer(transfer_buffer_id);
  return PP_TRUE;
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::GetState() {
  return GetCommandBuffer()->GetState();
}

scoped_refptr<gpu::Buffer> PPB_Graphics3D_Impl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  return GetCommandBuffer()->CreateTransferBuffer(size, id);
}

PP_Bool PPB_Graphics3D_Impl::DestroyTransferBuffer(int32_t id) {
  GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Graphics3D_Impl::Flush(int32_t put_offset) {
  GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  GetCommandBuffer()->WaitForTokenInRange(start, end);
  return GetCommandBuffer()->GetLastState();
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForGetOffsetInRange(
    int32_t start,
    int32_t end) {
  GetCommandBuffer()->WaitForGetOffsetInRange(start, end);
  return GetCommandBuffer()->GetLastState();
}

uint32_t PPB_Graphics3D_Impl::InsertSyncPoint() {
  return platform_context_->GetGpuControl()->InsertSyncPoint();
}

bool PPB_Graphics3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

bool PPB_Graphics3D_Impl::IsOpaque() { return platform_context_->IsOpaque(); }

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

void PPB_Graphics3D_Impl::ViewFlushedPaint() {}

gpu::CommandBuffer* PPB_Graphics3D_Impl::GetCommandBuffer() {
  return platform_context_->GetCommandBuffer();
}

gpu::GpuControl* PPB_Graphics3D_Impl::GetGpuControl() {
  return platform_context_->GetGpuControl();
}

int32 PPB_Graphics3D_Impl::DoSwapBuffers() {
  // We do not have a GLES2 implementation when using an OOP proxy.
  // The plugin-side proxy is responsible for adding the SwapBuffers command
  // to the command buffer in that case.
  if (gles2_impl())
    gles2_impl()->SwapBuffers();

  // Since the backing texture has been updated, a new sync point should be
  // inserted.
  platform_context_->InsertSyncPointForBackingMailbox();

  if (bound_to_instance_) {
    // If we are bound to the instance, we need to ask the compositor
    // to commit our backing texture so that the graphics appears on the page.
    // When the backing texture will be committed we get notified via
    // ViewFlushedPaint().
    //
    // Don't need to check for NULL from GetPluginInstance since when we're
    // bound, we know our instance is valid.
    HostGlobals::Get()->GetInstance(pp_instance())->CommitBackingTexture();
    commit_pending_ = true;
  } else {
    // Wait for the command to complete on the GPU to allow for throttling.
    platform_context_->Echo(base::Bind(&PPB_Graphics3D_Impl::OnSwapBuffers,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  return PP_OK_COMPLETIONPENDING;
}

bool PPB_Graphics3D_Impl::Init(PPB_Graphics3D_API* share_context,
                               const int32_t* attrib_list) {
  if (!InitRaw(share_context, attrib_list))
    return false;

  gpu::CommandBuffer* command_buffer = GetCommandBuffer();
  if (!command_buffer->Initialize())
    return false;

  gpu::gles2::GLES2Implementation* share_gles2 = NULL;
  if (share_context) {
    share_gles2 =
        static_cast<PPB_Graphics3D_Shared*>(share_context)->gles2_impl();
  }

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize, share_gles2);
}

bool PPB_Graphics3D_Impl::InitRaw(PPB_Graphics3D_API* share_context,
                                  const int32_t* attrib_list) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return false;

  const WebPreferences& prefs =
      static_cast<RenderViewImpl*>(plugin_instance->GetRenderView())
          ->webkit_preferences();
  // 3D access might be disabled or blacklisted.
  if (!prefs.pepper_3d_enabled)
    return false;
  // If accelerated compositing of plugins is disabled, fail to create a 3D
  // context, because it won't be visible. This allows graceful fallback in the
  // modules.
  if (!prefs.accelerated_compositing_for_plugins_enabled)
    return false;

  platform_context_.reset(new PlatformContext3D);
  if (!platform_context_)
    return false;

  PlatformContext3D* share_platform_context = NULL;
  if (share_context) {
    PPB_Graphics3D_Impl* share_graphics =
        static_cast<PPB_Graphics3D_Impl*>(share_context);
    share_platform_context = share_graphics->platform_context();
  }

  if (!platform_context_->Init(attrib_list, share_platform_context))
    return false;

  platform_context_->SetContextLostCallback(base::Bind(
      &PPB_Graphics3D_Impl::OnContextLost, weak_ptr_factory_.GetWeakPtr()));

  platform_context_->SetOnConsoleMessageCallback(base::Bind(
      &PPB_Graphics3D_Impl::OnConsoleMessage, weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void PPB_Graphics3D_Impl::OnConsoleMessage(const std::string& message, int id) {
  if (!bound_to_instance_)
    return;
  WebPluginContainer* container =
      HostGlobals::Get()->GetInstance(pp_instance())->container();
  if (!container)
    return;
  WebFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  WebConsoleMessage console_message = WebConsoleMessage(
      WebConsoleMessage::LevelError, WebString(base::UTF8ToUTF16(message)));
  frame->addMessageToConsole(console_message);
}

void PPB_Graphics3D_Impl::OnSwapBuffers() {
  if (HasPendingSwap()) {
    // If we're off-screen, no need to trigger and wait for compositing.
    // Just send the swap-buffers ACK to the plugin immediately.
    commit_pending_ = false;
    SwapBuffersACK(PP_OK);
  }
}

void PPB_Graphics3D_Impl::OnContextLost() {
  // Don't need to check for NULL from GetPluginInstance since when we're
  // bound, we know our instance is valid.
  if (bound_to_instance_) {
    HostGlobals::Get()->GetInstance(pp_instance())->BindGraphics(pp_instance(),
                                                                 0);
  }

  // Send context lost to plugin. This may have been caused by a PPAPI call, so
  // avoid re-entering.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PPB_Graphics3D_Impl::SendContextLost,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Graphics3D_Impl::SendContextLost() {
  // By the time we run this, the instance may have been deleted, or in the
  // process of being deleted. Even in the latter case, we don't want to send a
  // callback after DidDestroy.
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!instance || !instance->container())
    return;

  // This PPB_Graphics3D_Impl could be deleted during the call to
  // GetPluginInterface (which sends a sync message in some cases). We still
  // send the Graphics3DContextLost to the plugin; the instance may care about
  // that event even though this context has been destroyed.
  PP_Instance this_pp_instance = pp_instance();
  const PPP_Graphics3D* ppp_graphics_3d = static_cast<const PPP_Graphics3D*>(
      instance->module()->GetPluginInterface(PPP_GRAPHICS_3D_INTERFACE));
  // We have to check *again* that the instance exists, because it could have
  // been deleted during GetPluginInterface(). Even the PluginModule could be
  // deleted, but in that case, the instance should also be gone, so the
  // GetInstance check covers both cases.
  if (ppp_graphics_3d && HostGlobals::Get()->GetInstance(this_pp_instance))
    ppp_graphics_3d->Graphics3DContextLost(this_pp_instance);
}

}  // namespace content
