// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

#include <algorithm>
#include <map>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace content {

namespace {

static base::LazyInstance<base::Lock>::Leaky
    g_default_share_groups_lock = LAZY_INSTANCE_INITIALIZER;

typedef std::map<GpuChannelHost*,
    scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup> >
    ShareGroupMap;
static base::LazyInstance<ShareGroupMap> g_default_share_groups =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup>
    GetDefaultShareGroupForHost(GpuChannelHost* host) {
  base::AutoLock lock(g_default_share_groups_lock.Get());

  ShareGroupMap& share_groups = g_default_share_groups.Get();
  ShareGroupMap::iterator it = share_groups.find(host);
  if (it == share_groups.end()) {
    scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup> group =
        new WebGraphicsContext3DCommandBufferImpl::ShareGroup();
    share_groups[host] = group;
    return group;
  }
  return it->second;
}

uint32_t GenFlushID() {
  static base::subtle::Atomic32 flush_id = 0;

  base::subtle::Atomic32 my_id = base::subtle::Barrier_AtomicIncrement(
      &flush_id, 1);
  return static_cast<uint32_t>(my_id);
}

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    gles2::Initialize();
  }

  ~GLES2Initializer() {
    gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

////////////////////////////////////////////////////////////////////////////////

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////

} // namespace anonymous

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                    \
void WebGraphicsContext3DCommandBufferImpl::name() {                    \
  gl_->glname();                                                        \
}

#define DELEGATE_TO_GL_R(name, glname, rt)                              \
rt WebGraphicsContext3DCommandBufferImpl::name() {                      \
  return gl_->glname();                                                 \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                              \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {               \
  gl_->glname(a1);                                                      \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                         \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {                 \
  return gl_->glname(a1);                                               \
}

#define DELEGATE_TO_GL_1RB(name, glname, t1, rt)                        \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {                 \
  return gl_->glname(a1) ? true : false;                                \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                          \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {        \
  gl_->glname(a1, a2);                                                  \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                     \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {          \
  return gl_->glname(a1, a2);                                           \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                      \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3) { \
  gl_->glname(a1, a2, a3);                                              \
}

#define DELEGATE_TO_GL_3R(name, glname, t1, t2, t3, rt)                 \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3) {   \
  return gl_->glname(a1, a2, a3);                                       \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                  \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4) {               \
  gl_->glname(a1, a2, a3, a4);                                          \
}

#define DELEGATE_TO_GL_4R(name, glname, t1, t2, t3, t4, rt)             \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,     \
                                               t4 a4) {                 \
  return gl_->glname(a1, a2, a3, a4);                                   \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)              \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5) {        \
  gl_->glname(a1, a2, a3, a4, a5);                                      \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)          \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6) { \
  gl_->glname(a1, a2, a3, a4, a5, a6);                                  \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)      \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6,   \
                                                 t7 a7) {               \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7);                              \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)  \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6,   \
                                                 t7 a7, t8 a8) {        \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8);                          \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6,   \
                                                 t7 a7, t8 a8, t9 a9) { \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                      \
}

class WebGraphicsContext3DErrorMessageCallback
    : public gpu::gles2::GLES2Implementation::ErrorMessageCallback {
 public:
  WebGraphicsContext3DErrorMessageCallback(
      WebGraphicsContext3DCommandBufferImpl* context)
      : graphics_context_(context) {
  }

  virtual void OnErrorMessage(const char* msg, int id) OVERRIDE;

 private:
  WebGraphicsContext3DCommandBufferImpl* graphics_context_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DErrorMessageCallback);
};

void WebGraphicsContext3DErrorMessageCallback::OnErrorMessage(
    const char* msg, int id) {
  graphics_context_->OnErrorMessage(msg, id);
}

WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits::SharedMemoryLimits()
    : command_buffer_size(kDefaultCommandBufferSize),
      start_transfer_buffer_size(kDefaultStartTransferBufferSize),
      min_transfer_buffer_size(kDefaultMinTransferBufferSize),
      max_transfer_buffer_size(kDefaultMaxTransferBufferSize),
      mapped_memory_reclaim_limit(gpu::gles2::GLES2Implementation::kNoLimit) {}

WebGraphicsContext3DCommandBufferImpl::ShareGroup::ShareGroup() {
}

WebGraphicsContext3DCommandBufferImpl::ShareGroup::~ShareGroup() {
  DCHECK(contexts_.empty());
}

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    int surface_id,
    const GURL& active_url,
    GpuChannelHost* host,
    const Attributes& attributes,
    bool lose_context_when_out_of_memory,
    const SharedMemoryLimits& limits,
    WebGraphicsContext3DCommandBufferImpl* share_context)
    : initialize_failed_(false),
      visible_(false),
      host_(host),
      surface_id_(surface_id),
      active_url_(active_url),
      context_lost_callback_(0),
      context_lost_reason_(GL_NO_ERROR),
      error_message_callback_(0),
      attributes_(attributes),
      gpu_preference_(attributes.preferDiscreteGPU ? gfx::PreferDiscreteGpu
                                                   : gfx::PreferIntegratedGpu),
      weak_ptr_factory_(this),
      initialized_(false),
      gl_(NULL),
      lose_context_when_out_of_memory_(lose_context_when_out_of_memory),
      mem_limits_(limits),
      flush_id_(0) {
  if (share_context) {
    DCHECK(!attributes_.shareResources);
    share_group_ = share_context->share_group_;
  } else {
    share_group_ = attributes_.shareResources
        ? GetDefaultShareGroupForHost(host)
        : scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup>(
            new ShareGroup());
  }
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (real_gl_) {
    real_gl_->SetErrorMessageCallback(NULL);
  }

  Destroy();
}

bool WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL() {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::MaybeInitializeGL");

  if (!CreateContext(surface_id_ != 0)) {
    Destroy();
    initialize_failed_ = true;
    return false;
  }

  // TODO(twiz):  This code is too fragile in that it assumes that only WebGL
  // contexts will request noExtensions.
  if (gl_ && attributes_.noExtensions)
    gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  command_buffer_->SetChannelErrorCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnGpuChannelLost,
                 weak_ptr_factory_.GetWeakPtr()));

  command_buffer_->SetOnConsoleMessageCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnErrorMessage,
                 weak_ptr_factory_.GetWeakPtr()));

  client_error_message_callback_.reset(
      new WebGraphicsContext3DErrorMessageCallback(this));
  real_gl_->SetErrorMessageCallback(client_error_message_callback_.get());

  // Set attributes_ from created offscreen context.
  {
    static const int pcount = 4;
    static const GLenum pnames[pcount] = {
      GL_ALPHA_BITS,
      GL_DEPTH_BITS,
      GL_STENCIL_BITS,
      GL_SAMPLE_BUFFERS,
    };
    GLint pvalues[pcount] = { 0, 0, 0, 0 };

    gl_->GetMultipleIntegervCHROMIUM(pnames, pcount,
                                     pvalues, sizeof(pvalues));

    attributes_.alpha = pvalues[0] > 0;
    attributes_.depth = pvalues[1] > 0;
    attributes_.stencil = pvalues[2] > 0;
    attributes_.antialias = pvalues[3] > 0;
  }

  visible_ = true;
  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeCommandBuffer(
    bool onscreen, WebGraphicsContext3DCommandBufferImpl* share_context) {
  if (!host_.get())
    return false;

  CommandBufferProxyImpl* share_group_command_buffer = NULL;

  if (share_context) {
    share_group_command_buffer = share_context->command_buffer_.get();
  }

  std::vector<int32> attribs;
  attribs.push_back(ALPHA_SIZE);
  attribs.push_back(attributes_.alpha ? 8 : 0);
  attribs.push_back(DEPTH_SIZE);
  attribs.push_back(attributes_.depth ? 24 : 0);
  attribs.push_back(STENCIL_SIZE);
  attribs.push_back(attributes_.stencil ? 8 : 0);
  attribs.push_back(SAMPLES);
  attribs.push_back(attributes_.antialias ? 4 : 0);
  attribs.push_back(SAMPLE_BUFFERS);
  attribs.push_back(attributes_.antialias ? 1 : 0);
  attribs.push_back(FAIL_IF_MAJOR_PERF_CAVEAT);
  attribs.push_back(attributes_.failIfMajorPerformanceCaveat ? 1 : 0);
  attribs.push_back(LOSE_CONTEXT_WHEN_OUT_OF_MEMORY);
  attribs.push_back(lose_context_when_out_of_memory_ ? 1 : 0);
  attribs.push_back(BIND_GENERATES_RESOURCES);
  attribs.push_back(0);
  attribs.push_back(NONE);

  // Create a proxy to a command buffer in the GPU process.
  if (onscreen) {
    command_buffer_.reset(host_->CreateViewCommandBuffer(
        surface_id_,
        share_group_command_buffer,
        attribs,
        active_url_,
        gpu_preference_));
  } else {
    command_buffer_.reset(host_->CreateOffscreenCommandBuffer(
        gfx::Size(1, 1),
        share_group_command_buffer,
        attribs,
        active_url_,
        gpu_preference_));
  }

  if (!command_buffer_) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    return false;
  }

  DVLOG_IF(1, gpu::error::IsError(command_buffer_->GetLastError()))
      << "Context dead on arrival. Last error: "
      << command_buffer_->GetLastError();
  // Initialize the command buffer.
  bool result = command_buffer_->Initialize();
  LOG_IF(ERROR, !result) << "CommandBufferProxy::Initialize failed.";
  return result;
}

bool WebGraphicsContext3DCommandBufferImpl::CreateContext(bool onscreen) {
  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  scoped_refptr<gpu::gles2::ShareGroup> gles2_share_group;

  scoped_ptr<base::AutoLock> share_group_lock;
  bool add_to_share_group = false;
  if (!command_buffer_) {
    WebGraphicsContext3DCommandBufferImpl* share_context = NULL;

    share_group_lock.reset(new base::AutoLock(share_group_->lock()));
    share_context = share_group_->GetAnyContextLocked();

    if (!InitializeCommandBuffer(onscreen, share_context)) {
      LOG(ERROR) << "Failed to initialize command buffer.";
      return false;
    }

    if (share_context)
      gles2_share_group = share_context->GetImplementation()->share_group();

    add_to_share_group = true;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(mem_limits_.command_buffer_size)) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
    return false;
  }

  if (attributes_.noAutomaticFlushes)
    gles2_helper_->SetAutomaticFlushes(false);
  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_ .reset(new gpu::TransferBuffer(gles2_helper_.get()));

  DCHECK(host_.get());

  // Create the object exposing the OpenGL API.
  bool bind_generates_resources = false;
  real_gl_.reset(
      new gpu::gles2::GLES2Implementation(gles2_helper_.get(),
                                          gles2_share_group,
                                          transfer_buffer_.get(),
                                          bind_generates_resources,
                                          lose_context_when_out_of_memory_,
                                          command_buffer_.get()));
  gl_ = real_gl_.get();

  if (!real_gl_->Initialize(
      mem_limits_.start_transfer_buffer_size,
      mem_limits_.min_transfer_buffer_size,
      mem_limits_.max_transfer_buffer_size,
      mem_limits_.mapped_memory_reclaim_limit)) {
    LOG(ERROR) << "Failed to initialize GLES2Implementation.";
    return false;
  }

  if (add_to_share_group)
    share_group_->AddContextLocked(this);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGpuClientTracing)) {
    trace_gl_.reset(new gpu::gles2::GLES2TraceImplementation(gl_));
    gl_ = trace_gl_.get();
  }
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::makeContextCurrent() {
  if (!MaybeInitializeGL()) {
    DLOG(ERROR) << "Failed to initialize context.";
    return false;
  }
  gles2::SetGLContext(gl_);
  if (gpu::error::IsError(command_buffer_->GetLastError())) {
    LOG(ERROR) << "Context dead on arrival. Last error: "
               << command_buffer_->GetLastError();
    return false;
  }

  return true;
}

uint32_t WebGraphicsContext3DCommandBufferImpl::lastFlushID() {
  return flush_id_;
}

DELEGATE_TO_GL_R(insertSyncPoint, InsertSyncPointCHROMIUM, unsigned int)

void WebGraphicsContext3DCommandBufferImpl::Destroy() {
  share_group_->RemoveContext(this);

  if (gl_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gl_->Flush();
    gl_ = NULL;
  }

  trace_gl_.reset();
  real_gl_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  real_gl_.reset();

  if (command_buffer_) {
    if (host_.get())
      host_->DestroyCommandBuffer(command_buffer_.release());
    command_buffer_.reset();
  }

  host_ = NULL;
}

gpu::ContextSupport*
WebGraphicsContext3DCommandBufferImpl::GetContextSupport() {
  return real_gl_.get();
}

void WebGraphicsContext3DCommandBufferImpl::prepareTexture() {
  NOTREACHED();
}

void WebGraphicsContext3DCommandBufferImpl::postSubBufferCHROMIUM(
    int x, int y, int width, int height) {
  NOTREACHED();
}

DELEGATE_TO_GL_3(reshapeWithScaleFactor, ResizeCHROMIUM, int, int, float)

void WebGraphicsContext3DCommandBufferImpl::synthesizeGLError(
    WGC3Denum error) {
  if (std::find(synthetic_errors_.begin(), synthetic_errors_.end(), error) ==
      synthetic_errors_.end()) {
    synthetic_errors_.push_back(error);
  }
}

DELEGATE_TO_GL_4R(mapBufferSubDataCHROMIUM, MapBufferSubDataCHROMIUM, WGC3Denum,
                  WGC3Dintptr, WGC3Dsizeiptr, WGC3Denum, void*)

DELEGATE_TO_GL_1(unmapBufferSubDataCHROMIUM, UnmapBufferSubDataCHROMIUM,
                 const void*)

void* WebGraphicsContext3DCommandBufferImpl::mapTexSubImage2DCHROMIUM(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Dint xoffset,
    WGC3Dint yoffset,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    WGC3Denum access) {
  return gl_->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

DELEGATE_TO_GL_1(unmapTexSubImage2DCHROMIUM, UnmapTexSubImage2DCHROMIUM,
                 const void*)

void WebGraphicsContext3DCommandBufferImpl::setVisibilityCHROMIUM(
    bool visible) {
  NOTREACHED();
}

DELEGATE_TO_GL_3(discardFramebufferEXT, DiscardFramebufferEXT, WGC3Denum,
                 WGC3Dsizei, const WGC3Denum*)

void WebGraphicsContext3DCommandBufferImpl::copyTextureToParentTextureCHROMIUM(
    WebGLId texture, WebGLId parentTexture) {
  NOTIMPLEMENTED();
}

blink::WebString WebGraphicsContext3DCommandBufferImpl::
    getRequestableExtensionsCHROMIUM() {
  return blink::WebString::fromUTF8(
      gl_->GetRequestableExtensionsCHROMIUM());
}

DELEGATE_TO_GL_1(requestExtensionCHROMIUM, RequestExtensionCHROMIUM,
                 const char*)

void WebGraphicsContext3DCommandBufferImpl::blitFramebufferCHROMIUM(
    WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
    WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
    WGC3Dbitfield mask, WGC3Denum filter) {
  gl_->BlitFramebufferCHROMIUM(
      srcX0, srcY0, srcX1, srcY1,
      dstX0, dstY0, dstX1, dstY1,
      mask, filter);
}

DELEGATE_TO_GL_5(renderbufferStorageMultisampleCHROMIUM,
                 RenderbufferStorageMultisampleCHROMIUM, WGC3Denum, WGC3Dsizei,
                 WGC3Denum, WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_1(activeTexture, ActiveTexture, WGC3Denum)

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId,
                 WGC3Duint, const WGC3Dchar*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, WGC3Denum, WebGLId)

DELEGATE_TO_GL_2(bindFramebuffer, BindFramebuffer, WGC3Denum, WebGLId)

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbuffer, WGC3Denum, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, WGC3Denum, WebGLId)

DELEGATE_TO_GL_4(blendColor, BlendColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, WGC3Denum)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 WGC3Denum, WGC3Dsizeiptr, const void*, WGC3Denum)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 WGC3Denum, WGC3Dintptr, WGC3Dsizeiptr, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatus,
                  WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1(clear, Clear, WGC3Dbitfield)

DELEGATE_TO_GL_4(clearColor, ClearColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearDepth, ClearDepthf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, WGC3Dint)

DELEGATE_TO_GL_4(colorMask, ColorMask,
                 WGC3Dboolean, WGC3Dboolean, WGC3Dboolean, WGC3Dboolean)

DELEGATE_TO_GL_1(compileShader, CompileShader, WebGLId)

DELEGATE_TO_GL_8(compressedTexImage2D, CompressedTexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei, const void*)

DELEGATE_TO_GL_9(compressedTexSubImage2D, CompressedTexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint,
                 WGC3Denum, WGC3Dsizei, const void*)

DELEGATE_TO_GL_8(copyTexImage2D, CopyTexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei, WGC3Dint)

DELEGATE_TO_GL_8(copyTexSubImage2D, CopyTexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_1(cullFace, CullFace, WGC3Denum)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, WGC3Denum)

DELEGATE_TO_GL_1(depthMask, DepthMask, WGC3Dboolean)

DELEGATE_TO_GL_2(depthRange, DepthRangef, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, WGC3Denum)

DELEGATE_TO_GL_1(disableVertexAttribArray, DisableVertexAttribArray,
                 WGC3Duint)

DELEGATE_TO_GL_3(drawArrays, DrawArrays, WGC3Denum, WGC3Dint, WGC3Dsizei)

void WebGraphicsContext3DCommandBufferImpl::drawElements(WGC3Denum mode,
                                                         WGC3Dsizei count,
                                                         WGC3Denum type,
                                                         WGC3Dintptr offset) {
  gl_->DrawElements(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, WGC3Denum)

DELEGATE_TO_GL_1(enableVertexAttribArray, EnableVertexAttribArray,
                 WGC3Duint)

void WebGraphicsContext3DCommandBufferImpl::finish() {
  flush_id_ = GenFlushID();
  gl_->Finish();
}

void WebGraphicsContext3DCommandBufferImpl::flush() {
  flush_id_ = GenFlushID();
  gl_->Flush();
}

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbuffer,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2D,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId, WGC3Dint)

DELEGATE_TO_GL_1(frontFace, FrontFace, WGC3Denum)

DELEGATE_TO_GL_1(generateMipmap, GenerateMipmap, WGC3Denum)

bool WebGraphicsContext3DCommandBufferImpl::getActiveAttrib(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  if (!program) {
    synthesizeGLError(GL_INVALID_VALUE);
    return false;
  }
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  if (!name) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveAttrib(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = blink::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::getActiveUniform(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  if (!name) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveUniform(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = blink::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

DELEGATE_TO_GL_4(getAttachedShaders, GetAttachedShaders,
                 WebGLId, WGC3Dsizei, WGC3Dsizei*, WebGLId*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, WGC3Denum, WGC3Dboolean*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

blink::WebGraphicsContext3D::Attributes
WebGraphicsContext3DCommandBufferImpl::getContextAttributes() {
  return attributes_;
}

WGC3Denum WebGraphicsContext3DCommandBufferImpl::getError() {
  if (!synthetic_errors_.empty()) {
    std::vector<WGC3Denum>::iterator iter = synthetic_errors_.begin();
    WGC3Denum err = *iter;
    synthetic_errors_.erase(iter);
    return err;
  }

  return gl_->GetError();
}

bool WebGraphicsContext3DCommandBufferImpl::isContextLost() {
  return initialize_failed_ ||
      (command_buffer_ && IsCommandBufferContextLost()) ||
      context_lost_reason_ != GL_NO_ERROR;
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_4(getFramebufferAttachmentParameteriv,
                 GetFramebufferAttachmentParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_2(getIntegerv, GetIntegerv, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, WGC3Denum, WGC3Dint*)

blink::WebString WebGraphicsContext3DCommandBufferImpl::getProgramInfoLog(
    WebGLId program) {
  GLint logLength = 0;
  gl_->GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetProgramInfoLog(
      program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, WGC3Denum, WGC3Dint*)

blink::WebString WebGraphicsContext3DCommandBufferImpl::getShaderInfoLog(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderInfoLog(
      shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_4(getShaderPrecisionFormat, GetShaderPrecisionFormat,
                 WGC3Denum, WGC3Denum, WGC3Dint*, WGC3Dint*)

blink::WebString WebGraphicsContext3DCommandBufferImpl::getShaderSource(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderSource(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return blink::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DCommandBufferImpl::
    getTranslatedShaderSourceANGLE(WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(
      shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetTranslatedShaderSourceANGLE(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return blink::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DCommandBufferImpl::getString(
    WGC3Denum name) {
  return blink::WebString::fromUTF8(
      reinterpret_cast<const char*>(gl_->GetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv,
                 WGC3Denum, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, WGC3Dint, WGC3Dfloat*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, WGC3Dint, WGC3Dint*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 WGC3Duint, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 WGC3Duint, WGC3Denum, WGC3Dint*)

WGC3Dsizeiptr WebGraphicsContext3DCommandBufferImpl::getVertexAttribOffset(
    WGC3Duint index, WGC3Denum pname) {
  GLvoid* value = NULL;
  // NOTE: If pname is ever a value that returns more then 1 element
  // this will corrupt memory.
  gl_->GetVertexAttribPointerv(index, pname, &value);
  return static_cast<WGC3Dsizeiptr>(reinterpret_cast<intptr_t>(value));
}

DELEGATE_TO_GL_2(hint, Hint, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, WGC3Denum, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1(lineWidth, LineWidth, WGC3Dfloat)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_7(readPixels, ReadPixels,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei, WGC3Denum,
                 WGC3Denum, void*)

void WebGraphicsContext3DCommandBufferImpl::releaseShaderCompiler() {
}

DELEGATE_TO_GL_4(renderbufferStorage, RenderbufferStorage,
                 WGC3Denum, WGC3Denum, WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, WGC3Dfloat, WGC3Dboolean)

DELEGATE_TO_GL_4(scissor, Scissor, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

void WebGraphicsContext3DCommandBufferImpl::shaderSource(
    WebGLId shader, const WGC3Dchar* string) {
  GLint length = strlen(string);
  gl_->ShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_1(stencilMask, StencilMask, WGC3Duint)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 WGC3Denum, WGC3Duint)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_9(texImage2D, TexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei,
                 WGC3Dint, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf,
                 WGC3Denum, WGC3Denum, WGC3Dfloat);

static const unsigned int kTextureWrapR = 0x8072;

void WebGraphicsContext3DCommandBufferImpl::texParameteri(
    WGC3Denum target, WGC3Denum pname, WGC3Dint param) {
  // TODO(kbr): figure out whether the setting of TEXTURE_WRAP_R in
  // GraphicsContext3D.cpp is strictly necessary to avoid seams at the
  // edge of cube maps, and, if it is, push it into the GLES2 service
  // side code.
  if (pname == kTextureWrapR) {
    return;
  }
  gl_->TexParameteri(target, pname, param);
}

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, WGC3Dint, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, WGC3Dint, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, WGC3Dint,
                 WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, WGC3Duint, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, WGC3Duint,
                 const WGC3Dfloat*)

void WebGraphicsContext3DCommandBufferImpl::vertexAttribPointer(
    WGC3Duint index, WGC3Dint size, WGC3Denum type, WGC3Dboolean normalized,
    WGC3Dsizei stride, WGC3Dintptr offset) {
  gl_->VertexAttribPointer(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_2(genBuffers, GenBuffers, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(genFramebuffers, GenFramebuffers, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(genRenderbuffers, GenRenderbuffers, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(genTextures, GenTextures, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(deleteBuffers, DeleteBuffers, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(deleteFramebuffers, DeleteFramebuffers, WGC3Dsizei, WebGLId*);

DELEGATE_TO_GL_2(deleteRenderbuffers, DeleteRenderbuffers, WGC3Dsizei,
                 WebGLId*);

DELEGATE_TO_GL_2(deleteTextures, DeleteTextures, WGC3Dsizei, WebGLId*);

WebGLId WebGraphicsContext3DCommandBufferImpl::createBuffer() {
  GLuint o;
  gl_->GenBuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createFramebuffer() {
  GLuint o = 0;
  gl_->GenFramebuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createRenderbuffer() {
  GLuint o;
  gl_->GenRenderbuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createTexture() {
  GLuint o;
  gl_->GenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DCommandBufferImpl::deleteBuffer(WebGLId buffer) {
  gl_->DeleteBuffers(1, &buffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteFramebuffer(
    WebGLId framebuffer) {
  gl_->DeleteFramebuffers(1, &framebuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteRenderbuffer(
    WebGLId renderbuffer) {
  gl_->DeleteRenderbuffers(1, &renderbuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteTexture(WebGLId texture) {
  gl_->DeleteTextures(1, &texture);
}

DELEGATE_TO_GL_R(createProgram, CreateProgram, WebGLId)

DELEGATE_TO_GL_1R(createShader, CreateShader, WGC3Denum, WebGLId)

DELEGATE_TO_GL_1(deleteProgram, DeleteProgram, WebGLId)

DELEGATE_TO_GL_1(deleteShader, DeleteShader, WebGLId)

void WebGraphicsContext3DCommandBufferImpl::setErrorMessageCallback(
    WebGraphicsContext3D::WebGraphicsErrorMessageCallback* cb) {
  error_message_callback_ = cb;
}

void WebGraphicsContext3DCommandBufferImpl::setContextLostCallback(
    WebGraphicsContext3D::WebGraphicsContextLostCallback* cb) {
  context_lost_callback_ = cb;
}

WGC3Denum WebGraphicsContext3DCommandBufferImpl::getGraphicsResetStatusARB() {
  if (IsCommandBufferContextLost() &&
      context_lost_reason_ == GL_NO_ERROR) {
    return GL_UNKNOWN_CONTEXT_RESET_ARB;
  }

  return context_lost_reason_;
}

bool WebGraphicsContext3DCommandBufferImpl::IsCommandBufferContextLost() {
  // If the channel shut down unexpectedly, let that supersede the
  // command buffer's state.
  if (host_.get() && host_->IsLost())
    return true;
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  return state.error == gpu::error::kLostContext;
}

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
    GpuChannelHost* host,
    const WebGraphicsContext3D::Attributes& attributes,
    bool lose_context_when_out_of_memory,
    const GURL& active_url,
    const SharedMemoryLimits& limits,
    WebGraphicsContext3DCommandBufferImpl* share_context) {
  if (!host)
    return NULL;

  if (share_context && share_context->IsCommandBufferContextLost())
    return NULL;

  return new WebGraphicsContext3DCommandBufferImpl(
      0,
      active_url,
      host,
      attributes,
      lose_context_when_out_of_memory,
      limits,
      share_context);
}

DELEGATE_TO_GL_5(texImageIOSurface2DCHROMIUM, TexImageIOSurface2DCHROMIUM,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Duint, WGC3Duint)

DELEGATE_TO_GL_5(texStorage2DEXT, TexStorage2DEXT,
                 WGC3Denum, WGC3Dint, WGC3Duint, WGC3Dint, WGC3Dint)

WebGLId WebGraphicsContext3DCommandBufferImpl::createQueryEXT() {
  GLuint o;
  gl_->GenQueriesEXT(1, &o);
  return o;
}

void WebGraphicsContext3DCommandBufferImpl::deleteQueryEXT(
    WebGLId query) {
  gl_->DeleteQueriesEXT(1, &query);
}

DELEGATE_TO_GL_1R(isQueryEXT, IsQueryEXT, WebGLId, WGC3Dboolean)
DELEGATE_TO_GL_2(beginQueryEXT, BeginQueryEXT, WGC3Denum, WebGLId)
DELEGATE_TO_GL_1(endQueryEXT, EndQueryEXT, WGC3Denum)
DELEGATE_TO_GL_3(getQueryivEXT, GetQueryivEXT, WGC3Denum, WGC3Denum, WGC3Dint*)
DELEGATE_TO_GL_3(getQueryObjectuivEXT, GetQueryObjectuivEXT,
                 WebGLId, WGC3Denum, WGC3Duint*)

DELEGATE_TO_GL_6(copyTextureCHROMIUM, CopyTextureCHROMIUM,  WGC3Denum,
                 WebGLId, WebGLId, WGC3Dint, WGC3Denum, WGC3Denum);

DELEGATE_TO_GL_3(bindUniformLocationCHROMIUM, BindUniformLocationCHROMIUM,
                 WebGLId, WGC3Dint, const WGC3Dchar*)

void WebGraphicsContext3DCommandBufferImpl::shallowFlushCHROMIUM() {
  flush_id_ = GenFlushID();
  gl_->ShallowFlushCHROMIUM();
}

void WebGraphicsContext3DCommandBufferImpl::shallowFinishCHROMIUM() {
  flush_id_ = GenFlushID();
  gl_->ShallowFinishCHROMIUM();
}

DELEGATE_TO_GL_1(waitSyncPoint, WaitSyncPointCHROMIUM, GLuint)

void WebGraphicsContext3DCommandBufferImpl::loseContextCHROMIUM(
    WGC3Denum current, WGC3Denum other) {
  gl_->LoseContextCHROMIUM(current, other);
  gl_->Flush();
}

DELEGATE_TO_GL_1(genMailboxCHROMIUM, GenMailboxCHROMIUM, WGC3Dbyte*)
DELEGATE_TO_GL_2(produceTextureCHROMIUM, ProduceTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)
DELEGATE_TO_GL_2(consumeTextureCHROMIUM, ConsumeTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)

void WebGraphicsContext3DCommandBufferImpl::insertEventMarkerEXT(
    const WGC3Dchar* marker) {
  gl_->InsertEventMarkerEXT(0, marker);
}

void WebGraphicsContext3DCommandBufferImpl::pushGroupMarkerEXT(
    const WGC3Dchar* marker) {
  gl_->PushGroupMarkerEXT(0, marker);
}

DELEGATE_TO_GL(popGroupMarkerEXT, PopGroupMarkerEXT);

WebGLId WebGraphicsContext3DCommandBufferImpl::createVertexArrayOES() {
  GLuint array;
  gl_->GenVertexArraysOES(1, &array);
  return array;
}

void WebGraphicsContext3DCommandBufferImpl::deleteVertexArrayOES(
    WebGLId array) {
  gl_->DeleteVertexArraysOES(1, &array);
}

DELEGATE_TO_GL_1R(isVertexArrayOES, IsVertexArrayOES, WebGLId, WGC3Dboolean)
DELEGATE_TO_GL_1(bindVertexArrayOES, BindVertexArrayOES, WebGLId)

DELEGATE_TO_GL_2(bindTexImage2DCHROMIUM, BindTexImage2DCHROMIUM,
                 WGC3Denum, WGC3Dint)
DELEGATE_TO_GL_2(releaseTexImage2DCHROMIUM, ReleaseTexImage2DCHROMIUM,
                 WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_2R(mapBufferCHROMIUM, MapBufferCHROMIUM, WGC3Denum, WGC3Denum,
                  void*)
DELEGATE_TO_GL_1R(unmapBufferCHROMIUM, UnmapBufferCHROMIUM, WGC3Denum,
                  WGC3Dboolean)

DELEGATE_TO_GL_9(asyncTexImage2DCHROMIUM, AsyncTexImage2DCHROMIUM, WGC3Denum,
                 WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei, WGC3Dint,
                 WGC3Denum, WGC3Denum, const void*)
DELEGATE_TO_GL_9(asyncTexSubImage2DCHROMIUM, AsyncTexSubImage2DCHROMIUM,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_1(waitAsyncTexImage2DCHROMIUM, WaitAsyncTexImage2DCHROMIUM,
                 WGC3Denum)

DELEGATE_TO_GL_2(drawBuffersEXT, DrawBuffersEXT, WGC3Dsizei, const WGC3Denum*)

DELEGATE_TO_GL_4(drawArraysInstancedANGLE, DrawArraysInstancedANGLE, WGC3Denum,
                 WGC3Dint, WGC3Dsizei, WGC3Dsizei)

void WebGraphicsContext3DCommandBufferImpl::drawElementsInstancedANGLE(
    WGC3Denum mode,
    WGC3Dsizei count,
    WGC3Denum type,
    WGC3Dintptr offset,
    WGC3Dsizei primcount) {
  gl_->DrawElementsInstancedANGLE(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)), primcount);
}

DELEGATE_TO_GL_2(vertexAttribDivisorANGLE, VertexAttribDivisorANGLE, WGC3Duint,
                 WGC3Duint)

DELEGATE_TO_GL_3R(createImageCHROMIUM, CreateImageCHROMIUM,
                  WGC3Dsizei, WGC3Dsizei, WGC3Denum,
                  WGC3Duint);

DELEGATE_TO_GL_1(destroyImageCHROMIUM, DestroyImageCHROMIUM, WGC3Duint);

DELEGATE_TO_GL_3(getImageParameterivCHROMIUM, GetImageParameterivCHROMIUM,
                 WGC3Duint, WGC3Denum, GLint*);

DELEGATE_TO_GL_2R(mapImageCHROMIUM, MapImageCHROMIUM,
                  WGC3Duint, WGC3Denum, void*);

DELEGATE_TO_GL_1(unmapImageCHROMIUM, UnmapImageCHROMIUM, WGC3Duint);

DELEGATE_TO_GL_6(framebufferTexture2DMultisampleEXT,
                 FramebufferTexture2DMultisampleEXT,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId, WGC3Dint, WGC3Dsizei)

DELEGATE_TO_GL_5(renderbufferStorageMultisampleEXT,
                 RenderbufferStorageMultisampleEXT, WGC3Denum, WGC3Dsizei,
                 WGC3Denum, WGC3Dsizei, WGC3Dsizei)

GrGLInterface* WebGraphicsContext3DCommandBufferImpl::createGrGLInterface() {
  makeContextCurrent();
  return skia_bindings::CreateCommandBufferSkiaGLBinding();
}

namespace {

WGC3Denum convertReason(gpu::error::ContextLostReason reason) {
  switch (reason) {
  case gpu::error::kGuilty:
    return GL_GUILTY_CONTEXT_RESET_ARB;
  case gpu::error::kInnocent:
    return GL_INNOCENT_CONTEXT_RESET_ARB;
  case gpu::error::kUnknown:
    return GL_UNKNOWN_CONTEXT_RESET_ARB;
  }

  NOTREACHED();
  return GL_UNKNOWN_CONTEXT_RESET_ARB;
}

}  // anonymous namespace

void WebGraphicsContext3DCommandBufferImpl::OnGpuChannelLost() {
  context_lost_reason_ = convertReason(
      command_buffer_->GetLastState().context_lost_reason);
  if (context_lost_callback_) {
    context_lost_callback_->onContextLost();
  }

  share_group_->RemoveAllContexts();

  DCHECK(host_.get());
  {
    base::AutoLock lock(g_default_share_groups_lock.Get());
    g_default_share_groups.Get().erase(host_.get());
  }
}

void WebGraphicsContext3DCommandBufferImpl::OnErrorMessage(
    const std::string& message, int id) {
  if (error_message_callback_) {
    blink::WebString str = blink::WebString::fromUTF8(message.c_str());
    error_message_callback_->onErrorMessage(str, id);
  }
}

}  // namespace content
