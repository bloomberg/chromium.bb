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
#include <set>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/command_buffer_proxy.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "webkit/gpu/gl_bindings_skia_cmd_buffer.h"

namespace content {
static base::LazyInstance<base::Lock>::Leaky
    g_all_shared_contexts_lock = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<std::set<WebGraphicsContext3DCommandBufferImpl*> >
    g_all_shared_contexts = LAZY_INSTANCE_INITIALIZER;

namespace {

void ClearSharedContextsIfInShareSet(
    WebGraphicsContext3DCommandBufferImpl* context) {
  // If the given context isn't in the share set, that means that it
  // or another context it was previously sharing with already
  // provoked a lost context. Other contexts might have since been
  // successfully created and added to the share set, so do not clear
  // out the share set unless we know that all the contexts in there
  // are supposed to be lost simultaneously.
  base::AutoLock lock(g_all_shared_contexts_lock.Get());
  std::set<WebGraphicsContext3DCommandBufferImpl*>* share_set =
      g_all_shared_contexts.Pointer();
  for (std::set<WebGraphicsContext3DCommandBufferImpl*>::iterator iter =
           share_set->begin(); iter != share_set->end(); ++iter) {
    if (context == *iter) {
      share_set->clear();
      return;
    }
  }
}

size_t ClampUint64ToSizeT(uint64 value) {
  value = std::min(value,
                   static_cast<uint64>(std::numeric_limits<size_t>::max()));
  return static_cast<size_t>(value);
}

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 1 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

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

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    int surface_id,
    const GURL& active_url,
    GpuChannelHostFactory* factory,
    const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client)
    : initialize_failed_(false),
      factory_(factory),
      visible_(false),
      free_command_buffer_when_invisible_(false),
      host_(NULL),
      surface_id_(surface_id),
      active_url_(active_url),
      swap_client_(swap_client),
      context_lost_callback_(0),
      context_lost_reason_(GL_NO_ERROR),
      error_message_callback_(0),
      swapbuffers_complete_callback_(0),
      gpu_preference_(gfx::PreferIntegratedGpu),
      cached_width_(0),
      cached_height_(0),
      bound_fbo_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      initialized_(false),
      parent_(NULL),
      parent_texture_id_(0),
      command_buffer_(NULL),
      gles2_helper_(NULL),
      transfer_buffer_(NULL),
      gl_(NULL),
      real_gl_(NULL),
      trace_gl_(NULL),
      frame_number_(0),
      bind_generates_resources_(false),
      use_echo_for_swap_ack_(true) {
#if (defined(OS_MACOSX) || defined(OS_WIN)) && !defined(USE_AURA)
  // Get ViewMsg_SwapBuffers_ACK from browser for single-threaded path.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  use_echo_for_swap_ack_ =
      command_line.HasSwitch(switches::kEnableThreadedCompositing);
#endif
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (real_gl_) {
    real_gl_->SetErrorMessageCallback(NULL);
  }

  {
    base::AutoLock lock(g_all_shared_contexts_lock.Get());
    g_all_shared_contexts.Pointer()->erase(this);
  }
  Destroy();
}

void WebGraphicsContext3DCommandBufferImpl::InitializeWithCommandBuffer(
    CommandBufferProxyImpl* command_buffer,
    const WebGraphicsContext3D::Attributes& attributes,
    bool bind_generates_resources) {
  DCHECK(command_buffer);
  command_buffer_ = command_buffer;
  attributes_ = attributes;
  bind_generates_resources_ = bind_generates_resources;
}

bool WebGraphicsContext3DCommandBufferImpl::Initialize(
    const WebGraphicsContext3D::Attributes& attributes,
    bool bind_generates_resources,
    CauseForGpuLaunch cause) {
  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::initialize");

  attributes_ = attributes;
  bind_generates_resources_ = bind_generates_resources;
  DCHECK(!command_buffer_);

  if (!factory_)
    return false;

  if (attributes.preferDiscreteGPU)
    gpu_preference_ = gfx::PreferDiscreteGpu;

  host_ = factory_->EstablishGpuChannelSync(cause);
  if (!host_)
    return false;
  DCHECK(host_->state() == GpuChannelHost::kConnected);

  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL(
    const char* allowed_extensions) {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::MaybeInitializeGL");

  const char* preferred_extensions = "*";

  if (!CreateContext(surface_id_ != 0,
                     allowed_extensions ?
                         allowed_extensions : preferred_extensions)) {
    Destroy();
    return false;
  }

  // TODO(twiz):  This code is too fragile in that it assumes that only WebGL
  // contexts will request noExtensions.
  if (gl_ && attributes_.noExtensions)
    gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  command_buffer_->SetChannelErrorCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnContextLost,
                 weak_ptr_factory_.GetWeakPtr()));

  command_buffer_->SetOnConsoleMessageCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnErrorMessage,
                 weak_ptr_factory_.GetWeakPtr()));

  client_error_message_callback_.reset(
      new WebGraphicsContext3DErrorMessageCallback(this));
  real_gl_->SetErrorMessageCallback(client_error_message_callback_.get());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  free_command_buffer_when_invisible_ =
      command_line.HasSwitch(switches::kEnablePruneGpuCommandBuffers);

  // Set attributes_ from created offscreen context.
  {
    GLint alpha_bits = 0;
    getIntegerv(GL_ALPHA_BITS, &alpha_bits);
    attributes_.alpha = alpha_bits > 0;
    GLint depth_bits = 0;
    getIntegerv(GL_DEPTH_BITS, &depth_bits);
    attributes_.depth = depth_bits > 0;
    GLint stencil_bits = 0;
    getIntegerv(GL_STENCIL_BITS, &stencil_bits);
    attributes_.stencil = stencil_bits > 0;
    GLint sample_buffers = 0;
    getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
    attributes_.antialias = sample_buffers > 0;
  }

  if (attributes_.shareResources) {
    base::AutoLock lock(g_all_shared_contexts_lock.Get());
    g_all_shared_contexts.Pointer()->insert(this);
  }

  visible_ = true;
  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeCommandBuffer(
    bool onscreen,
    const char* allowed_extensions) {
  if (!host_)
    return false;
  // We need to lock g_all_shared_contexts to ensure that the context we picked
  // for our share group isn't deleted.
  // (There's also a lock in our destructor.)
  base::AutoLock lock(g_all_shared_contexts_lock.Get());
  CommandBufferProxyImpl* share_group = NULL;
  if (attributes_.shareResources) {
    WebGraphicsContext3DCommandBufferImpl* share_group_context =
        g_all_shared_contexts.Pointer()->empty() ?
            NULL : *g_all_shared_contexts.Pointer()->begin();
    share_group = share_group_context ?
        share_group_context->command_buffer_ : NULL;
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
  attribs.push_back(NONE);

  // Create a proxy to a command buffer in the GPU process.
  if (onscreen) {
    command_buffer_ = host_->CreateViewCommandBuffer(
        surface_id_,
        share_group,
        allowed_extensions,
        attribs,
        active_url_,
        gpu_preference_);
  } else {
    command_buffer_ = host_->CreateOffscreenCommandBuffer(
        gfx::Size(1, 1),
        share_group,
        allowed_extensions,
        attribs,
        active_url_,
        gpu_preference_);
  }

  if (!command_buffer_)
    return false;

  // Initialize the command buffer.
  return command_buffer_->Initialize();
}

bool WebGraphicsContext3DCommandBufferImpl::CreateContext(
    bool onscreen,
    const char* allowed_extensions) {

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  if (!command_buffer_ &&
      !InitializeCommandBuffer(onscreen,
                               allowed_extensions)) {
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  if (!gles2_helper_->Initialize(kCommandBufferSize))
    return false;

  if (attributes_.noAutomaticFlushes)
    gles2_helper_->SetAutomaticFlushes(false);

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_ = new gpu::TransferBuffer(gles2_helper_);

  WebGraphicsContext3DCommandBufferImpl* share_group_context =
      g_all_shared_contexts.Pointer()->empty() ?
          NULL : *g_all_shared_contexts.Pointer()->begin();

  // Create the object exposing the OpenGL API.
  real_gl_ = new gpu::gles2::GLES2Implementation(
      gles2_helper_,
      share_group_context ?
          share_group_context->GetImplementation()->share_group() : NULL,
      transfer_buffer_,
      attributes_.shareResources,
      bind_generates_resources_);
  gl_ = real_gl_;

  if (!real_gl_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) {
    return false;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGpuClientTracing)) {
    trace_gl_ = new gpu::gles2::GLES2TraceImplementation(gl_);
    gl_ = trace_gl_;
  }

  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::makeContextCurrent() {
  if (!MaybeInitializeGL(NULL))
    return false;
  gles2::SetGLContext(gl_);
  if (command_buffer_->GetLastError() != gpu::error::kNoError)
    return false;

  return true;
}

int WebGraphicsContext3DCommandBufferImpl::width() {
  return cached_width_;
}

int WebGraphicsContext3DCommandBufferImpl::height() {
  return cached_height_;
}

bool WebGraphicsContext3DCommandBufferImpl::isGLES2Compliant() {
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::setParentContext(
    WebGraphicsContext3D* parent_context) {
  WebGraphicsContext3DCommandBufferImpl* parent_context_impl =
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(parent_context);
  return SetParent(parent_context_impl);
}

unsigned int WebGraphicsContext3DCommandBufferImpl::insertSyncPoint() {
  return gl_->InsertSyncPointCHROMIUM();
}

bool WebGraphicsContext3DCommandBufferImpl::SetParent(
    WebGraphicsContext3DCommandBufferImpl* new_parent) {
  if (parent_ == new_parent)
    return true;

  // Allocate a texture ID with respect to the parent and change the parent.
  uint32 new_parent_texture_id = 0;
  if (command_buffer_) {
    if (new_parent) {
      // Flush any remaining commands in the parent context to make sure the
      // texture id accounting stays consistent.
      int32 token = new_parent->gles2_helper_->InsertToken();
      new_parent->gles2_helper_->WaitForToken(token);
      new_parent_texture_id =
        new_parent->real_gl_->MakeTextureId();

      if (!command_buffer_->SetParent(new_parent->command_buffer_,
                                      new_parent_texture_id)) {
        new_parent->real_gl_->FreeTextureId(parent_texture_id_);
        return false;
      }
    } else {
      if (!command_buffer_->SetParent(NULL, 0))
        return false;
    }
  }

  // Free the previous parent's texture ID.
  if (parent_ && parent_texture_id_ != 0) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    gpu::gles2::GLES2Implementation* parent_gles2 =
        parent_->real_gl_;
    parent_gles2->helper()->CommandBufferHelper::Finish();
    parent_gles2->FreeTextureId(parent_texture_id_);
  }

  if (new_parent) {
    parent_ = new_parent;
    parent_texture_id_ = new_parent_texture_id;
  } else {
    parent_ = NULL;
    parent_texture_id_ = 0;
  }

  return true;
}

void WebGraphicsContext3DCommandBufferImpl::Destroy() {
  SetParent(NULL);

  if (gl_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gl_->Flush();
    gl_ = NULL;
  }

  if (trace_gl_) {
    delete trace_gl_;
    trace_gl_ = NULL;
  }

  if (real_gl_) {
    delete real_gl_;
    real_gl_ = NULL;
  }

  if (transfer_buffer_) {
    delete transfer_buffer_;
    transfer_buffer_ = NULL;
  }

  delete gles2_helper_;
  gles2_helper_ = NULL;

  if (command_buffer_) {
    if (host_)
      host_->DestroyCommandBuffer(command_buffer_);
    else
      delete command_buffer_;
    command_buffer_ = NULL;
  }

  host_ = NULL;
}

// TODO(apatrick,piman): This should be renamed to something clearer.
int WebGraphicsContext3DCommandBufferImpl::GetGPUProcessID() {
  return host_ ? host_->gpu_host_id() : 0;
}

int WebGraphicsContext3DCommandBufferImpl::GetChannelID() {
  return host_ ? host_->client_id() : 0;
}

int WebGraphicsContext3DCommandBufferImpl::GetContextID() {
  return command_buffer_->GetRouteID();
}

WebGLId WebGraphicsContext3DCommandBufferImpl::getPlatformTextureId() {
  return parent_texture_id_;
}

void WebGraphicsContext3DCommandBufferImpl::prepareTexture() {
  TRACE_EVENT1("gpu",
                "WebGraphicsContext3DCommandBufferImpl::SwapBuffers",
                "frame", frame_number_);
  frame_number_++;
  // Copies the contents of the off-screen render target into the texture
  // used by the compositor.
  if (ShouldUseSwapClient())
    swap_client_->OnViewContextSwapBuffersPosted();

  if (command_buffer_->GetLastState().error == gpu::error::kNoError)
    gl_->SwapBuffers();

  if (use_echo_for_swap_ack_) {
    command_buffer_->Echo(base::Bind(
        &WebGraphicsContext3DCommandBufferImpl::OnSwapBuffersComplete,
        weak_ptr_factory_.GetWeakPtr()));
  }
#if defined(OS_MACOSX)
  // It appears that making the compositor's on-screen context current on
  // other platforms implies this flush. TODO(kbr): this means that the
  // TOUCH build and, in the future, other platforms might need this.
  gl_->Flush();
#endif
}

void WebGraphicsContext3DCommandBufferImpl::postSubBufferCHROMIUM(
    int x, int y, int width, int height) {
  // Same flow control as WebGraphicsContext3DCommandBufferImpl::prepareTexture
  // (see above).
  if (ShouldUseSwapClient())
    swap_client_->OnViewContextSwapBuffersPosted();
  gl_->PostSubBufferCHROMIUM(x, y, width, height);
  command_buffer_->Echo(base::Bind(
      &WebGraphicsContext3DCommandBufferImpl::OnSwapBuffersComplete,
      weak_ptr_factory_.GetWeakPtr()));
}

void WebGraphicsContext3DCommandBufferImpl::reshape(int width, int height) {
  cached_width_ = width;
  cached_height_ = height;

  gl_->ResizeCHROMIUM(width, height);
}

void WebGraphicsContext3DCommandBufferImpl::FlipVertically(
    uint8* framebuffer,
    unsigned int width,
    unsigned int height) {
  if (width == 0)
    return;
  scanline_.resize(width * 4);
  uint8* scanline = &scanline_[0];
  unsigned int row_bytes = width * 4;
  unsigned int count = height / 2;
  for (unsigned int i = 0; i < count; i++) {
    uint8* row_a = framebuffer + i * row_bytes;
    uint8* row_b = framebuffer + (height - i - 1) * row_bytes;
    // TODO(kbr): this is where the multiplication of the alpha
    // channel into the color buffer will need to occur if the
    // user specifies the "premultiplyAlpha" flag in the context
    // creation attributes.
    memcpy(scanline, row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline, row_bytes);
  }
}

bool WebGraphicsContext3DCommandBufferImpl::readBackFramebuffer(
    unsigned char* pixels,
    size_t buffer_size,
    WebGLId buffer,
    int width,
    int height) {
  if (buffer_size != static_cast<size_t>(4 * width * height)) {
    return false;
  }

  // Earlier versions of this code used the GPU to flip the
  // framebuffer vertically before reading it back for compositing
  // via software. This code was quite complicated, used a lot of
  // GPU memory, and didn't provide an obvious speedup. Since this
  // vertical flip is only a temporary solution anyway until Chrome
  // is fully GPU composited, it wasn't worth the complexity.

  bool mustRestoreFBO = (bound_fbo_ != buffer);
  if (mustRestoreFBO) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, buffer);
  }
  gl_->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
  // Swizzle red and blue channels to match SkBitmap's byte ordering.
  // TODO(kbr): expose GL_BGRA as extension.
  for (size_t i = 0; i < buffer_size; i += 4) {
    std::swap(pixels[i], pixels[i + 2]);
  }
#endif

  if (mustRestoreFBO) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, bound_fbo_);
  }

  if (pixels) {
    FlipVertically(pixels, width, height);
  }

  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::readBackFramebuffer(
    unsigned char* pixels,
    size_t buffer_size) {
  return readBackFramebuffer(pixels, buffer_size, 0, width(), height());
}

void WebGraphicsContext3DCommandBufferImpl::synthesizeGLError(
    WGC3Denum error) {
  if (std::find(synthetic_errors_.begin(), synthetic_errors_.end(), error) ==
      synthetic_errors_.end()) {
    synthetic_errors_.push_back(error);
  }
}

void* WebGraphicsContext3DCommandBufferImpl::mapBufferSubDataCHROMIUM(
    WGC3Denum target,
    WGC3Dintptr offset,
    WGC3Dsizeiptr size,
    WGC3Denum access) {
  return gl_->MapBufferSubDataCHROMIUM(target, offset, size, access);
}

void WebGraphicsContext3DCommandBufferImpl::unmapBufferSubDataCHROMIUM(
    const void* mem) {
  return gl_->UnmapBufferSubDataCHROMIUM(mem);
}

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

void WebGraphicsContext3DCommandBufferImpl::unmapTexSubImage2DCHROMIUM(
    const void* mem) {
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
}

void WebGraphicsContext3DCommandBufferImpl::setVisibilityCHROMIUM(
    bool visible) {
  gl_->Flush();
  visible_ = visible;
  command_buffer_->SetSurfaceVisible(visible);
  if (!visible)
    real_gl_->FreeEverything();
}

void WebGraphicsContext3DCommandBufferImpl::discardFramebufferEXT(
    WGC3Denum target, WGC3Dsizei numAttachments, const WGC3Denum* attachments) {
  gl_->DiscardFramebufferEXT(target, numAttachments, attachments);
}

void WebGraphicsContext3DCommandBufferImpl::discardBackbufferCHROMIUM() {
  gl_->Flush();
  command_buffer_->DiscardBackbuffer();
}

void WebGraphicsContext3DCommandBufferImpl::ensureBackbufferCHROMIUM() {
  gl_->Flush();
  command_buffer_->EnsureBackbuffer();
}

void WebGraphicsContext3DCommandBufferImpl::sendManagedMemoryStatsCHROMIUM(
    const WebGraphicsManagedMemoryStats* stats)
{
  CHECK(command_buffer_);
  command_buffer_->SendManagedMemoryStats(GpuManagedMemoryStats(
      stats->bytesVisible,
      stats->bytesVisibleAndNearby,
      stats->bytesAllocated,
      stats->backbufferRequested));
}

void WebGraphicsContext3DCommandBufferImpl::
    setMemoryAllocationChangedCallbackCHROMIUM(
        WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {
  if (!command_buffer_)
    return;

  if (callback)
    command_buffer_->SetMemoryAllocationChangedCallback(base::Bind(
        &WebGraphicsContext3DCommandBufferImpl::OnMemoryAllocationChanged,
        weak_ptr_factory_.GetWeakPtr(),
        callback));
  else
    command_buffer_->SetMemoryAllocationChangedCallback(
        base::Callback<void(const GpuMemoryAllocationForRenderer&)>());
}


void WebGraphicsContext3DCommandBufferImpl::copyTextureToParentTextureCHROMIUM(
    WebGLId texture, WebGLId parentTexture) {
  NOTIMPLEMENTED();
}

void WebGraphicsContext3DCommandBufferImpl::
    rateLimitOffscreenContextCHROMIUM() {
  gl_->RateLimitOffscreenContextCHROMIUM();
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::
    getRequestableExtensionsCHROMIUM() {
  return WebKit::WebString::fromUTF8(
      gl_->GetRequestableExtensionsCHROMIUM());
}

void WebGraphicsContext3DCommandBufferImpl::requestExtensionCHROMIUM(
    const char* extension) {
  gl_->RequestExtensionCHROMIUM(extension);
}

void WebGraphicsContext3DCommandBufferImpl::blitFramebufferCHROMIUM(
    WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
    WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
    WGC3Dbitfield mask, WGC3Denum filter) {
  gl_->BlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1,
      dstX0, dstY0, dstX1, dstY1,
      mask, filter);
}

void WebGraphicsContext3DCommandBufferImpl::
    renderbufferStorageMultisampleCHROMIUM(
        WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
        WGC3Dsizei width, WGC3Dsizei height) {
  gl_->RenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createStreamTextureCHROMIUM(
    WebGLId texture) {
  return gl_->CreateStreamTextureCHROMIUM(texture);
}

void WebGraphicsContext3DCommandBufferImpl::destroyStreamTextureCHROMIUM(
    WebGLId texture) {
  gl_->DestroyStreamTextureCHROMIUM(texture);
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                    \
void WebGraphicsContext3DCommandBufferImpl::name() {                    \
  gl_->glname();                                                        \
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

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                  \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
  gl_->glname(a1, a2, a3, a4);                                          \
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
                                                 t4 a4, t5 a5, t6 a6, t7 a7) { \
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

DELEGATE_TO_GL_1(activeTexture, ActiveTexture, WGC3Denum)

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId,
                 WGC3Duint, const WGC3Dchar*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, WGC3Denum, WebGLId)

void WebGraphicsContext3DCommandBufferImpl::bindFramebuffer(
    WGC3Denum target,
    WebGLId framebuffer) {
  gl_->BindFramebuffer(target, framebuffer);
  bound_fbo_ = framebuffer;
}

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
  gl_->Finish();
  if (!visible_ && free_command_buffer_when_invisible_)
    real_gl_->FreeEverything();
}

void WebGraphicsContext3DCommandBufferImpl::flush() {
  gl_->Flush();
  if (!visible_ && free_command_buffer_when_invisible_)
    real_gl_->FreeEverything();
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
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
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
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
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
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
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
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
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

WebKit::WebGraphicsContext3D::Attributes
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

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getProgramInfoLog(
    WebGLId program) {
  GLint logLength = 0;
  gl_->GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetProgramInfoLog(
      program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, WGC3Denum, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderInfoLog(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderInfoLog(
      shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_4(getShaderPrecisionFormat, GetShaderPrecisionFormat,
                 WGC3Denum, WGC3Denum, WGC3Dint*, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderSource(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderSource(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return WebKit::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::
    getTranslatedShaderSourceANGLE(WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(
      shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetTranslatedShaderSourceANGLE(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return WebKit::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getString(
    WGC3Denum name) {
  return WebKit::WebString::fromUTF8(
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

WebGLId WebGraphicsContext3DCommandBufferImpl::createProgram() {
  return gl_->CreateProgram();
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createRenderbuffer() {
  GLuint o;
  gl_->GenRenderbuffers(1, &o);
  return o;
}

DELEGATE_TO_GL_1R(createShader, CreateShader, WGC3Denum, WebGLId);

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

void WebGraphicsContext3DCommandBufferImpl::deleteProgram(WebGLId program) {
  gl_->DeleteProgram(program);
}

void WebGraphicsContext3DCommandBufferImpl::deleteRenderbuffer(
    WebGLId renderbuffer) {
  gl_->DeleteRenderbuffers(1, &renderbuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteShader(WebGLId shader) {
  gl_->DeleteShader(shader);
}

void WebGraphicsContext3DCommandBufferImpl::deleteTexture(WebGLId texture) {
  gl_->DeleteTextures(1, &texture);
}

bool WebGraphicsContext3DCommandBufferImpl::ShouldUseSwapClient() {
  return factory_ && factory_->IsMainThread() && swap_client_.get();
}

void WebGraphicsContext3DCommandBufferImpl::OnSwapBuffersComplete() {
  typedef WebGraphicsContext3DSwapBuffersClient WGC3DSwapClient;
  // This may be called after tear-down of the RenderView.
  if (ShouldUseSwapClient()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &WGC3DSwapClient::OnViewContextSwapBuffersComplete, swap_client_));
  }

  if (swapbuffers_complete_callback_)
    swapbuffers_complete_callback_->onSwapBuffersComplete();
}

WebGraphicsMemoryAllocation::PriorityCutoff
    WebGraphicsContext3DCommandBufferImpl::WebkitPriorityCutoff(
        GpuMemoryAllocationForRenderer::PriorityCutoff priorityCutoff) {
  switch (priorityCutoff) {
  case GpuMemoryAllocationForRenderer::kPriorityCutoffAllowNothing:
    return WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing;
  case GpuMemoryAllocationForRenderer::kPriorityCutoffAllowOnlyRequired:
    return WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly;
  case GpuMemoryAllocationForRenderer::kPriorityCutoffAllowNiceToHave:
    return WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleAndNearby;
  case GpuMemoryAllocationForRenderer::kPriorityCutoffAllowEverything:
    return WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything;
  }
  NOTREACHED();
  return WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything;
}

void WebGraphicsContext3DCommandBufferImpl::OnMemoryAllocationChanged(
    WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback,
    const GpuMemoryAllocationForRenderer& allocation) {

  // Convert the gpu structure to the WebKit structure.
  WebGraphicsMemoryAllocation web_allocation;
  web_allocation.bytesLimitWhenVisible =
      ClampUint64ToSizeT(allocation.bytes_limit_when_visible);
  web_allocation.priorityCutoffWhenVisible =
      WebkitPriorityCutoff(allocation.priority_cutoff_when_visible);
  web_allocation.bytesLimitWhenNotVisible =
      ClampUint64ToSizeT(allocation.bytes_limit_when_not_visible);
  web_allocation.priorityCutoffWhenNotVisible =
      WebkitPriorityCutoff(allocation.priority_cutoff_when_not_visible);
  web_allocation.haveBackbufferWhenNotVisible =
      allocation.have_backbuffer_when_not_visible;
  web_allocation.enforceButDoNotKeepAsPolicy =
      allocation.enforce_but_do_not_keep_as_policy;

  // Populate deprecated WebKit fields. These may be removed when references to
  // them in WebKit are removed.
  web_allocation.gpuResourceSizeInBytes =
      ClampUint64ToSizeT(allocation.bytes_limit_when_visible);
  web_allocation.suggestHaveBackbuffer =
      allocation.have_backbuffer_when_not_visible;

  if (callback)
    callback->onMemoryAllocationChanged(web_allocation);

  // We may have allocated transfer buffers in order to free GL resources in a
  // backgrounded tab. Re-free the transfer buffers.
  if (!visible_)
    real_gl_->FreeEverything();
}

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
  if (host_ && host_->state() == GpuChannelHost::kLost)
    return true;
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  return state.error == gpu::error::kLostContext;
}

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateViewContext(
      GpuChannelHostFactory* factory,
      int32 surface_id,
      const char* allowed_extensions,
      const WebGraphicsContext3D::Attributes& attributes,
      bool bind_generates_resources,
      const GURL& active_url,
      CauseForGpuLaunch cause) {
  WebGraphicsContext3DCommandBufferImpl* context =
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          active_url,
          factory,
          base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>());
  if (!context->Initialize(attributes, bind_generates_resources, cause) ||
      !context->MaybeInitializeGL(allowed_extensions)) {
    delete context;
    return NULL;
  }
  return context;
}

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
    GpuChannelHostFactory* factory,
    const WebGraphicsContext3D::Attributes& attributes,
    const GURL& active_url) {
  if (!factory)
    return NULL;
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> null_client;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          0, active_url, factory, null_client));
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  if (context->Initialize(attributes, false, cause))
    return context.release();
  return NULL;
}

void WebGraphicsContext3DCommandBufferImpl::
    setSwapBuffersCompleteCallbackCHROMIUM(
    WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM* cb) {
  swapbuffers_complete_callback_ = cb;
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

DELEGATE_TO_GL_5(copyTextureCHROMIUM, CopyTextureCHROMIUM,  WGC3Denum,
                 WebGLId, WebGLId, WGC3Dint, WGC3Denum);

DELEGATE_TO_GL_3(bindUniformLocationCHROMIUM, BindUniformLocationCHROMIUM,
                 WebGLId, WGC3Dint, const WGC3Dchar*)

DELEGATE_TO_GL(shallowFlushCHROMIUM,ShallowFlushCHROMIUM);

DELEGATE_TO_GL_1(waitSyncPoint, WaitSyncPointCHROMIUM, GLuint)

void WebGraphicsContext3DCommandBufferImpl::genMailboxCHROMIUM(
    WGC3Dbyte* name) {
  std::vector<gpu::Mailbox> names(1);
  if (command_buffer_->GenerateMailboxNames(1, &names))
    memcpy(name, names[0].name, GL_MAILBOX_SIZE_CHROMIUM);
  else
    synthesizeGLError(GL_OUT_OF_MEMORY);
}

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

void* WebGraphicsContext3DCommandBufferImpl::mapBufferCHROMIUM(
    WGC3Denum target, WGC3Denum access) {
  return gl_->MapBufferCHROMIUM(target, access);
}

WGC3Dboolean WebGraphicsContext3DCommandBufferImpl::unmapBufferCHROMIUM(
    WGC3Denum target) {
  return gl_->UnmapBufferCHROMIUM(target);
}

void WebGraphicsContext3DCommandBufferImpl::asyncTexImage2DCHROMIUM(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Denum internalformat,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Dint border,
    WGC3Denum format,
    WGC3Denum type,
    const void* pixels) {
  return gl_->AsyncTexImage2DCHROMIUM(
      target, level, internalformat,
      width, height, border, format, type, pixels);
}

void WebGraphicsContext3DCommandBufferImpl::asyncTexSubImage2DCHROMIUM(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Dint xoffset,
    WGC3Dint yoffset,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    const void *pixels) {
  return gl_->AsyncTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset,
      width, height, format, type, pixels);
}

void WebGraphicsContext3DCommandBufferImpl::waitAsyncTexImage2DCHROMIUM(
    WGC3Denum target) {
  return gl_->WaitAsyncTexImage2DCHROMIUM(target);
}

void WebGraphicsContext3DCommandBufferImpl::drawBuffersEXT(
    WGC3Dsizei n,
    const WGC3Denum* bufs) {
  gl_->DrawBuffersEXT(n, bufs);
}

GrGLInterface* WebGraphicsContext3DCommandBufferImpl::onCreateGrGLInterface() {
  return webkit::gpu::CreateCommandBufferSkiaGLBinding();
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

void WebGraphicsContext3DCommandBufferImpl::OnContextLost() {
  context_lost_reason_ = convertReason(
      command_buffer_->GetLastState().context_lost_reason);
  if (context_lost_callback_) {
    context_lost_callback_->onContextLost();
  }
  if (attributes_.shareResources)
    ClearSharedContextsIfInShareSet(this);
  if (ShouldUseSwapClient())
    swap_client_->OnViewContextSwapBuffersAborted();
}

void WebGraphicsContext3DCommandBufferImpl::OnErrorMessage(
    const std::string& message, int id) {
  if (error_message_callback_) {
    WebKit::WebString str = WebKit::WebString::fromUTF8(message.c_str());
    error_message_callback_->onErrorMessage(str, id);
  }
}

}  // namespace content
