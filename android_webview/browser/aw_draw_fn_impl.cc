// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_draw_fn_impl.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/native_library.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "jni/AwDrawFnImpl_jni.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

using base::android::JavaParamRef;
using content::BrowserThread;

namespace android_webview {

namespace {
GLNonOwnedCompatibilityContext* g_gl_context = nullptr;
}

class GLNonOwnedCompatibilityContext : public gl::GLContextEGL {
 public:
  GLNonOwnedCompatibilityContext()
      : gl::GLContextEGL(nullptr),
        surface_(
            base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(gfx::Size(1, 1))) {
    gl::GLContextAttribs attribs;
    Initialize(surface_.get(), attribs);

    DCHECK(!g_gl_context);
    g_gl_context = this;
  }

  bool MakeCurrent(gl::GLSurface* surface) override {
    // A GLNonOwnedCompatibilityContext may have set the GetRealCurrent()
    // pointer to itself, while re-using our EGL context. In these cases just
    // call SetCurrent to restore expected GetRealContext().
    if (GetHandle() == eglGetCurrentContext()) {
      if (surface) {
        // No one should change the current EGL surface without also changing
        // the context.
        DCHECK(eglGetCurrentSurface(EGL_DRAW) == surface->GetHandle());
      }

      SetCurrent(surface);
      return true;
    }

    return gl::GLContextEGL::MakeCurrent(surface);
  }

  bool MakeCurrent() { return MakeCurrent(surface_.get()); }

  static scoped_refptr<GLNonOwnedCompatibilityContext> GetOrCreateInstance() {
    if (g_gl_context)
      return base::WrapRefCounted(g_gl_context);
    return base::WrapRefCounted(new GLNonOwnedCompatibilityContext);
  }

 private:
  ~GLNonOwnedCompatibilityContext() override {
    DCHECK_EQ(g_gl_context, this);
    g_gl_context = nullptr;
  }

  scoped_refptr<gl::GLSurface> surface_;
};

namespace {
VulkanState* g_vulkan_state = nullptr;
}

class VulkanState : public base::RefCounted<VulkanState> {
 public:
  static scoped_refptr<VulkanState> GetOrCreateInstance(
      AwDrawFn_InitVkParams* params) {
    if (g_vulkan_state) {
      DCHECK_EQ(params->device, g_vulkan_state->device());
      DCHECK_EQ(params->queue, g_vulkan_state->queue());
      return base::WrapRefCounted(g_vulkan_state);
    }

    auto new_state = base::WrapRefCounted(new VulkanState);
    if (!new_state->Initialize(params))
      return nullptr;

    return new_state;
  }

  VkPhysicalDevice physical_device() { return physical_device_; }
  VkDevice device() { return device_; }
  VkQueue queue() { return queue_; }
  gpu::VulkanImplementation* implementation() { return implementation_.get(); }
  GrContext* gr_context() { return gr_context_.get(); }

 private:
  friend class base::RefCounted<VulkanState>;

  VulkanState() {
    DCHECK_EQ(nullptr, g_vulkan_state);
    g_vulkan_state = this;
  }

  ~VulkanState() {
    DCHECK_EQ(g_vulkan_state, this);
    g_vulkan_state = nullptr;
  }

  bool Initialize(AwDrawFn_InitVkParams* params) {
    physical_device_ = params->physical_device;
    device_ = params->device;
    queue_ = params->queue;

    // Don't call init on implementation. Instead call InitVulkanForWebView,
    // which avoids creating a new instance.
    implementation_ = gpu::CreateVulkanImplementation();
    if (!InitVulkanForWebView(params->instance, params->device)) {
      LOG(ERROR) << "Unable to initialize Vulkan pointers.";
      return false;
    }

    // Create our Skia GrContext.
    GrVkGetProc get_proc =
        MakeUnifiedGetter(vkGetInstanceProcAddr, vkGetDeviceProcAddr);
    GrVkExtensions extensions;
    extensions.init(get_proc, params->instance, params->physical_device,
                    params->enabled_instance_extension_names_length,
                    params->enabled_instance_extension_names,
                    params->enabled_device_extension_names_length,
                    params->enabled_device_extension_names);
    GrVkBackendContext backend_context{
        .fInstance = params->instance,
        .fPhysicalDevice = params->physical_device,
        .fDevice = params->device,
        .fQueue = params->queue,
        .fGraphicsQueueIndex = params->graphics_queue_index,
        .fInstanceVersion = params->instance_version,
        .fVkExtensions = &extensions,
        .fDeviceFeatures = params->device_features,
        .fDeviceFeatures2 = params->device_features_2,
        .fMemoryAllocator = nullptr,
        .fGetProc = get_proc,
        .fOwnsInstanceAndDevice = false,
    };
    gr_context_ = GrContext::MakeVulkan(backend_context);
    if (!gr_context_) {
      LOG(ERROR) << "Unable to initialize GrContext.";
      return false;
    }
    return true;
  }

  static bool InitVulkanForWebView(VkInstance instance, VkDevice device) {
    gpu::VulkanFunctionPointers* vulkan_function_pointers =
        gpu::GetVulkanFunctionPointers();

    // If we are re-initing, we don't need to re-load the shared library or
    // re-bind unassociated pointers. These shouldn't change.
    if (!vulkan_function_pointers->vulkan_loader_library_) {
      base::NativeLibraryLoadError native_library_load_error;
      vulkan_function_pointers->vulkan_loader_library_ =
          base::LoadNativeLibrary(base::FilePath("libvulkan.so"),
                                  &native_library_load_error);
      if (!vulkan_function_pointers->vulkan_loader_library_)
        return false;
      if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
        return false;
    }

    // These vars depend on |instance| and |device| and should be
    // re-initialized.
    if (!vulkan_function_pointers->BindInstanceFunctionPointers(instance))
      return false;
    if (!vulkan_function_pointers->BindPhysicalDeviceFunctionPointers(instance))
      return false;
    if (!vulkan_function_pointers->BindDeviceFunctionPointers(device))
      return false;

    return true;
  }

  static GrVkGetProc MakeUnifiedGetter(const PFN_vkGetInstanceProcAddr& iproc,
                                       const PFN_vkGetDeviceProcAddr& dproc) {
    return [&iproc, &dproc](const char* proc_name, VkInstance instance,
                            VkDevice device) {
      if (device != VK_NULL_HANDLE) {
        return dproc(device, proc_name);
      }
      return iproc(instance, proc_name);
    };
  }

  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::unique_ptr<gpu::VulkanImplementation> implementation_;
  sk_sp<GrContext> gr_context_;
};

namespace {

AwDrawFnFunctionTable* g_draw_fn_function_table = nullptr;

void OnSyncWrapper(int functor, void* data, AwDrawFn_OnSyncParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnSync", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnSync(params);
}

void OnContextDestroyedWrapper(int functor, void* data) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnContextDestroyed",
               "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->OnContextDestroyed();
}

void OnDestroyedWrapper(int functor, void* data) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_OnDestroyed", "functor",
               functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  delete static_cast<AwDrawFnImpl*>(data);
}

void DrawGLWrapper(int functor, void* data, AwDrawFn_DrawGLParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_DrawGL", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawGL(params);
}

void InitVkWrapper(int functor, void* data, AwDrawFn_InitVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_InitVk", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->InitVk(params);
}

void DrawVkWrapper(int functor, void* data, AwDrawFn_DrawVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_DrawVk", "functor", functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->DrawVk(params);
}

void PostDrawVkWrapper(int functor,
                       void* data,
                       AwDrawFn_PostDrawVkParams* params) {
  TRACE_EVENT1("android_webview,toplevel", "DrawFn_PostDrawVk", "functor",
               functor);
  CHECK_EQ(static_cast<AwDrawFnImpl*>(data)->functor_handle(), functor);
  static_cast<AwDrawFnImpl*>(data)->PostDrawVk(params);
}

}  // namespace

static void JNI_AwDrawFnImpl_SetDrawFnFunctionTable(JNIEnv* env,
                                                    jlong function_table) {
  g_draw_fn_function_table =
      reinterpret_cast<AwDrawFnFunctionTable*>(function_table);
}

AwDrawFnImpl::AwDrawFnImpl()
    : render_thread_manager_(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_draw_fn_function_table);

  static AwDrawFnFunctorCallbacks g_functor_callbacks{
      &OnSyncWrapper,      &OnContextDestroyedWrapper,
      &OnDestroyedWrapper, &DrawGLWrapper,
      &InitVkWrapper,      &DrawVkWrapper,
      &PostDrawVkWrapper,
  };

  functor_handle_ =
      g_draw_fn_function_table->create_functor(this, &g_functor_callbacks);
}

AwDrawFnImpl::~AwDrawFnImpl() {}

void AwDrawFnImpl::ReleaseHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  render_thread_manager_.RemoveFromCompositorFrameProducerOnUI();
  g_draw_fn_function_table->release_functor(functor_handle_);
}

jint AwDrawFnImpl::GetFunctorHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return functor_handle_;
}

jlong AwDrawFnImpl::GetCompositorFrameConsumer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(GetCompositorFrameConsumer());
}

static jlong JNI_AwDrawFnImpl_Create(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(new AwDrawFnImpl());
}

void AwDrawFnImpl::OnSync(AwDrawFn_OnSyncParams* params) {
  render_thread_manager_.CommitFrameOnRT();
}

void AwDrawFnImpl::OnContextDestroyed() {
  // Try to make GL current to safely clean up. Ignore failures.
  if (gl_context_)
    gl_context_->MakeCurrent();

  {
    RenderThreadManager::InsideHardwareReleaseReset release_reset(
        &render_thread_manager_);
    render_thread_manager_.DestroyHardwareRendererOnRT(
        false /* save_restore */);
  }

  while (!in_flight_draws_.empty()) {
    // Let returned InFlightDraw go out of scope.
    TakeInFlightDrawForReUse();
  }

  vk_state_.reset();
  gl_context_.reset();
}

void AwDrawFnImpl::DrawGL(AwDrawFn_DrawGLParams* params) {
  struct HardwareRendererDrawParams hr_params {};
  hr_params.clip_left = params->clip_left;
  hr_params.clip_top = params->clip_top;
  hr_params.clip_right = params->clip_right;
  hr_params.clip_bottom = params->clip_bottom;
  hr_params.width = params->width;
  hr_params.height = params->height;
  hr_params.is_layer = params->is_layer;

  static_assert(base::size(decltype(params->transform){}) ==
                    base::size(hr_params.transform),
                "transform size mismatch");
  for (size_t i = 0; i < base::size(hr_params.transform); ++i) {
    hr_params.transform[i] = params->transform[i];
  }
  render_thread_manager_.DrawOnRT(false /* save_restore */, &hr_params);
}

void AwDrawFnImpl::InitVk(AwDrawFn_InitVkParams* params) {
  // We should never have a |vk_state_| if we are calling VkInit. This
  // means context destroyed was not correctly called.
  DCHECK(!vk_state_);
  vk_state_ = VulkanState::GetOrCreateInstance(params);

  // Make sure we have a GL context.
  DCHECK(!gl_context_);
  gl_context_ = GLNonOwnedCompatibilityContext::GetOrCreateInstance();
}

void AwDrawFnImpl::DrawVk(AwDrawFn_DrawVkParams* params) {
  if (!vk_state_ || !gl_context_)
    return;

  if (!gl_context_->MakeCurrent()) {
    LOG(ERROR) << "Failed to make GL context current for drawing.";
    return;
  }

  // If |pending_draw_| is non-null, we called DrawVk twice without PostDrawVk.
  DCHECK(!pending_draw_);

  // If we've exhausted our buffers, re-use an existing one.
  // TODO(ericrk): Benchmark using more than 1 buffer.
  if (in_flight_draws_.size() >= 1 /* single buffering */) {
    pending_draw_ = TakeInFlightDrawForReUse();
  }

  // If prev buffer is wrong size, just re-allocate.
  if (pending_draw_ && pending_draw_->ahb_image->GetSize() !=
                           gfx::Size(params->width, params->height)) {
    pending_draw_ = nullptr;
  }

  // If we weren't able to re-use a previous draw, create one.
  if (!pending_draw_) {
    pending_draw_ = std::make_unique<InFlightDraw>(vk_state_.get());

    AHardwareBuffer_Desc desc = {};
    desc.width = params->width;
    desc.height = params->height;
    desc.layers = 1;  // number of images
    // TODO(ericrk): Handle other formats.
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_NEVER |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER |
                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                 AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    AHardwareBuffer* buffer = nullptr;
    base::AndroidHardwareBufferCompat::GetInstance().Allocate(&desc, &buffer);
    if (!buffer) {
      LOG(ERROR) << "Failed to allocate AHardwareBuffer for WebView rendering.";
      return;
    }
    auto scoped_buffer =
        base::android::ScopedHardwareBufferHandle::Adopt(buffer);

    pending_draw_->ahb_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(
        gfx::Size(params->width, params->height));
    if (!pending_draw_->ahb_image->Initialize(scoped_buffer.get(),
                                              false /* preserved */)) {
      LOG(ERROR) << "Failed to initialize GLImage for AHardwareBuffer.";
      return;
    }

    glGenTextures(1, static_cast<GLuint*>(&pending_draw_->texture_id));
    GLenum target = GL_TEXTURE_2D;
    glBindTexture(target, pending_draw_->texture_id);
    if (!pending_draw_->ahb_image->BindTexImage(target)) {
      LOG(ERROR) << "Failed to bind GLImage for AHardwareBuffer.";
      return;
    }
    glBindTexture(target, 0);
    glGenFramebuffersEXT(1, &pending_draw_->framebuffer_id);
    glBindFramebufferEXT(GL_FRAMEBUFFER, pending_draw_->framebuffer_id);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, pending_draw_->texture_id, 0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "Failed to set up framebuffer for WebView GL drawing.";
      return;
    }
  }

  // Ask GL to wait on any Vk sync_fd before writing.
  gpu::InsertEglFenceAndWait(std::move(pending_draw_->sync_fd));

  // Bind buffer and render with GL.
  base::ScopedFD gl_done_fd;
  {
    glBindFramebufferEXT(GL_FRAMEBUFFER, pending_draw_->framebuffer_id);
    glViewport(0, 0, params->width, params->height);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    struct HardwareRendererDrawParams hr_params {};
    hr_params.clip_left = params->clip_left;
    hr_params.clip_top = params->clip_top;
    hr_params.clip_right = params->clip_right;
    hr_params.clip_bottom = params->clip_bottom;
    hr_params.width = params->width;
    hr_params.height = params->height;
    hr_params.is_layer = params->is_layer;

    static_assert(base::size(decltype(params->transform){}) ==
                      base::size(hr_params.transform),
                  "transform size mismatch");
    for (size_t i = 0; i < base::size(hr_params.transform); ++i) {
      hr_params.transform[i] = params->transform[i];
    }
    render_thread_manager_.DrawOnRT(false /* save_restore */, &hr_params);
    gl_done_fd = gpu::CreateEglFenceAndExportFd();
  }

  // Create a GrVkSecondaryCBDrawContext to render our AHB w/ Vulkan.
  // TODO(ericrk): Handle non-RGBA.
  skcms_TransferFunction transfer_fn{
      params->transfer_function_g, params->transfer_function_a,
      params->transfer_function_b, params->transfer_function_c,
      params->transfer_function_e, params->transfer_function_f};
  skcms_Matrix3x3 to_xyz;
  static_assert(sizeof(to_xyz.vals) == sizeof(params->color_space_toXYZD50),
                "Color space matrix sizes do not match");
  memcpy(&to_xyz.vals[0][0], &params->color_space_toXYZD50[0],
         sizeof(to_xyz.vals));
  sk_sp<SkColorSpace> color_space = SkColorSpace::MakeRGB(transfer_fn, to_xyz);
  // TODO(ericrk): Use colorspace.
  SkImageInfo info = SkImageInfo::MakeN32Premul(params->width, params->height);
  VkRect2D draw_bounds;
  GrVkDrawableInfo drawable_info{
      .fSecondaryCommandBuffer = params->secondary_command_buffer,
      .fColorAttachmentIndex = params->color_attachment_index,
      .fCompatibleRenderPass = params->compatible_render_pass,
      .fFormat = params->format,
      .fDrawBounds = &draw_bounds,
  };
  SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
  pending_draw_->draw_context = GrVkSecondaryCBDrawContext::Make(
      vk_state_->gr_context(), info, drawable_info, &props);

  // If we have a |gl_done_fd|, create a Skia GrBackendSemaphore from
  // |gl_done_fd| and wait.
  if (gl_done_fd.is_valid()) {
    VkSemaphore gl_done_semaphore;
    if (!vk_state_->implementation()->ImportSemaphoreFdKHR(
            vk_state_->device(), std::move(gl_done_fd), &gl_done_semaphore)) {
      LOG(ERROR) << "Could not create Vulkan semaphore for GL completion.";
      return;
    }
    GrBackendSemaphore gr_semaphore;
    gr_semaphore.initVulkan(gl_done_semaphore);
    if (!pending_draw_->draw_context->wait(1, &gr_semaphore)) {
      // If wait returns false, we must clean up the |gl_done_semaphore|.
      vkDestroySemaphore(vk_state_->device(), gl_done_semaphore, nullptr);
      LOG(ERROR) << "Could not wait on GL completion semaphore.";
      return;
    }
  }

  // Create a VkImage and import AHB.
  if (!pending_draw_->image_info.fImage) {
    VkImage vk_image;
    VkImageCreateInfo vk_image_info;
    VkDeviceMemory vk_device_memory;
    VkDeviceSize mem_allocation_size;
    if (!vk_state_->implementation()->CreateVkImageAndImportAHB(
            vk_state_->device(), vk_state_->physical_device(),
            gfx::Size(params->width, params->height),
            base::android::ScopedHardwareBufferHandle::Create(
                pending_draw_->ahb_image->GetAHardwareBuffer()->buffer()),
            &vk_image, &vk_image_info, &vk_device_memory,
            &mem_allocation_size)) {
      LOG(ERROR) << "Could not create VkImage from AHB.";
      return;
    }

    // Create backend texture from the VkImage.
    GrVkAlloc alloc = {vk_device_memory, 0, mem_allocation_size, 0};
    pending_draw_->image_info = {vk_image,
                                 alloc,
                                 vk_image_info.tiling,
                                 vk_image_info.initialLayout,
                                 vk_image_info.format,
                                 vk_image_info.mipLevels};
  }

  // Create an SkImage from AHB.
  GrBackendTexture backend_texture(params->width, params->height,
                                   pending_draw_->image_info);
  pending_draw_->ahb_skimage = SkImage::MakeFromTexture(
      vk_state_->gr_context(), backend_texture, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
  if (!pending_draw_->ahb_skimage) {
    LOG(ERROR) << "Could not create SkImage from VkImage.";
    return;
  }

  // Draw the SkImage.
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrcOver);
  pending_draw_->draw_context->getCanvas()->drawImage(
      pending_draw_->ahb_skimage, 0, 0, &paint);
  pending_draw_->draw_context->flush();
}

void AwDrawFnImpl::PostDrawVk(AwDrawFn_PostDrawVkParams* params) {
  if (!vk_state_ || !gl_context_)
    return;

  // Release the SkImage so that Skia transitions it back to EXTERNAL.
  pending_draw_->ahb_skimage.reset();

  // Create a semaphore to track the image's transition back to external.
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;
  VkResult result = vkCreateSemaphore(vk_state_->device(), &sem_info, nullptr,
                                      &pending_draw_->post_draw_semaphore);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Could not create VkSemaphore.";
    return;
  }
  GrBackendSemaphore gr_post_draw_semaphore;
  gr_post_draw_semaphore.initVulkan(pending_draw_->post_draw_semaphore);

  // Flush so that we know the image's transition has been submitted and that
  // the |post_draw_semaphore| is pending.
  GrSemaphoresSubmitted submitted =
      vk_state_->gr_context()->flushAndSignalSemaphores(
          1, &gr_post_draw_semaphore);
  if (submitted != GrSemaphoresSubmitted::kYes) {
    LOG(ERROR) << "Skia could not submit GrSemaphore.";
    return;
  }
  if (!vk_state_->implementation()->GetSemaphoreFdKHR(
          vk_state_->device(), pending_draw_->post_draw_semaphore,
          &pending_draw_->sync_fd)) {
    LOG(ERROR) << "Could not retrieve SyncFD from |post_draw_semaphore|.";
    return;
  }

  // Get a fence to wait on for CPU-side cleanup.
  VkFenceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  DCHECK(VK_NULL_HANDLE == pending_draw_->post_draw_fence);
  result = vkCreateFence(vk_state_->device(), &create_info, nullptr,
                         &pending_draw_->post_draw_fence);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Could not create VkFence.";
    return;
  }
  result = vkQueueSubmit(vk_state_->queue(), 0, nullptr,
                         pending_draw_->post_draw_fence);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Could not submit fence to queue.";
    return;
  }

  // Add the |pending_draw_| to |in_flight_draws_|.
  in_flight_draws_.push_back(std::move(pending_draw_));
}

std::unique_ptr<AwDrawFnImpl::InFlightDraw>
AwDrawFnImpl::TakeInFlightDrawForReUse() {
  DCHECK(vk_state_);
  DCHECK(!in_flight_draws_.empty());
  std::unique_ptr<InFlightDraw>& draw = in_flight_draws_.front();

  // Wait for our draw's |post_draw_fence| to pass.
  DCHECK(draw->post_draw_fence != VK_NULL_HANDLE);
  VkResult wait_result =
      vkWaitForFences(vk_state_->device(), 1, &draw->post_draw_fence, VK_TRUE,
                      base::TimeDelta::FromSeconds(60).InNanoseconds());
  if (wait_result != VK_SUCCESS) {
    LOG(ERROR) << "Fence did not pass in the expected timeframe.";
    return nullptr;
  }

  draw->draw_context->releaseResources();
  draw->draw_context.reset();
  vkDestroyFence(vk_state_->device(), draw->post_draw_fence, nullptr);
  draw->post_draw_fence = VK_NULL_HANDLE;
  vkDestroySemaphore(vk_state_->device(), draw->post_draw_semaphore, nullptr);
  draw->post_draw_semaphore = VK_NULL_HANDLE;

  std::unique_ptr<InFlightDraw> draw_to_return = std::move(draw);
  in_flight_draws_.pop_front();
  return draw_to_return;
}

AwDrawFnImpl::InFlightDraw::InFlightDraw(VulkanState* vk_state)
    : vk_state(vk_state) {}

AwDrawFnImpl::InFlightDraw::~InFlightDraw() {
  // If |draw_context| is valid, we encountered an error during Vk drawing and
  // should call vkQueueWaitIdle to ensure safe shutdown.
  bool encountered_error = !!draw_context;
  if (encountered_error) {
    // Clean up one-off objects which may have been left alive due to an error.

    // Clean up |ahb_skimage| first, as doing so generates Vk commands we need
    // to flush before the vkQueueWaitIdle below.
    if (ahb_skimage) {
      ahb_skimage.reset();
      vk_state->gr_context()->flush();
    }
    // We encountered an error and are not sure when our Vk objects are safe to
    // delete. VkQueueWaitIdle to ensure safety.
    vkQueueWaitIdle(vk_state->queue());
    if (draw_context) {
      draw_context->releaseResources();
      draw_context.reset();
    }
    if (post_draw_fence != VK_NULL_HANDLE) {
      vkDestroyFence(vk_state->device(), post_draw_fence, nullptr);
      post_draw_fence = VK_NULL_HANDLE;
    }
    if (post_draw_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(vk_state->device(), post_draw_semaphore, nullptr);
      post_draw_semaphore = VK_NULL_HANDLE;
    }
  }
  DCHECK(!draw_context);
  DCHECK(!ahb_skimage);
  DCHECK(post_draw_fence == VK_NULL_HANDLE);
  DCHECK(post_draw_semaphore == VK_NULL_HANDLE);

  // Clean up re-usable components that are expected to still be alive.
  if (texture_id)
    glDeleteTextures(1, &texture_id);
  if (framebuffer_id)
    glDeleteFramebuffersEXT(1, &framebuffer_id);
  if (image_info.fImage != VK_NULL_HANDLE) {
    vkDestroyImage(vk_state->device(), image_info.fImage, nullptr);
    vkFreeMemory(vk_state->device(), image_info.fAlloc.fMemory, nullptr);
  }
}

}  // namespace android_webview
