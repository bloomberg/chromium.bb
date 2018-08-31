// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_host_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "ipc/ipc_channel.h"
#include "ui/gfx/font_render_params.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace viz {
namespace {

// A wrapper around gfx::FontRenderParams that checks it is set and accessed on
// the same thread.
class FontRenderParams {
 public:
  void Set(const gfx::FontRenderParams& params);
  const base::Optional<gfx::FontRenderParams>& Get();

 private:
  friend class base::NoDestructor<FontRenderParams>;

  FontRenderParams();
  ~FontRenderParams();

  THREAD_CHECKER(thread_checker_);
  base::Optional<gfx::FontRenderParams> params_;

  DISALLOW_COPY_AND_ASSIGN(FontRenderParams);
};

void FontRenderParams::Set(const gfx::FontRenderParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params_ = params;
}

const base::Optional<gfx::FontRenderParams>& FontRenderParams::Get() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return params_;
}

FontRenderParams::FontRenderParams() = default;

FontRenderParams::~FontRenderParams() {
  NOTREACHED();
}

FontRenderParams& GetFontRenderParams() {
  static base::NoDestructor<FontRenderParams> instance;
  return *instance;
}

}  // namespace

GpuHostImpl::InitParams::InitParams() = default;

GpuHostImpl::InitParams::InitParams(InitParams&&) = default;

GpuHostImpl::InitParams::~InitParams() = default;

GpuHostImpl::GpuHostImpl(Delegate* delegate,
                         IPC::Channel* channel,
                         InitParams params)
    : delegate_(delegate),
      channel_(channel),
      params_(std::move(params)),
      gpu_host_binding_(this),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(channel_);
  channel_->GetAssociatedInterfaceSupport()->GetRemoteAssociatedInterface(
      &viz_main_ptr_);
  mojom::GpuHostPtr host_proxy;
  gpu_host_binding_.Bind(mojo::MakeRequest(&host_proxy));

  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr
      discardable_manager_ptr;
  auto discardable_request = mojo::MakeRequest(&discardable_manager_ptr);
  delegate_->BindDiscardableMemoryRequest(std::move(discardable_request));

  DCHECK(GetFontRenderParams().Get());
  viz_main_ptr_->CreateGpuService(
      mojo::MakeRequest(&gpu_service_ptr_), std::move(host_proxy),
      std::move(discardable_manager_ptr), activity_flags_.CloneHandle(),
      GetFontRenderParams().Get()->subpixel_rendering);
}

GpuHostImpl::~GpuHostImpl() = default;

// static
void GpuHostImpl::InitFontRenderParams(const gfx::FontRenderParams& params) {
  DCHECK(!GetFontRenderParams().Get());
  GetFontRenderParams().Set(params);
}

void GpuHostImpl::OnProcessLaunched(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(base::kNullProcessId, pid_);
  DCHECK_NE(base::kNullProcessId, pid);
  pid_ = pid;
}

void GpuHostImpl::OnProcessCrashed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the GPU process crashed while compiling a shader, we may have invalid
  // cached binaries. Completely clear the shader cache to force shader binaries
  // to be re-created.
  if (activity_flags_.IsFlagSet(
          gpu::ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY)) {
    auto* shader_cache_factory = delegate_->GetShaderCacheFactory();
    for (auto cache_key : client_id_to_shader_cache_) {
      // This call will temporarily extend the lifetime of the cache (kept
      // alive in the factory), and may drop loads of cached shader binaries if
      // it takes a while to complete. As we are intentionally dropping all
      // binaries, this behavior is fine.
      shader_cache_factory->ClearByClientId(
          cache_key.first, base::Time(), base::Time::Max(), base::DoNothing());
    }
  }
}

void GpuHostImpl::AddConnectionErrorHandler(base::OnceClosure handler) {
  connection_error_handlers_.push_back(std::move(handler));
}

void GpuHostImpl::BlockLiveOffscreenContexts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto iter = urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    delegate_->BlockDomainFrom3DAPIs(*iter, Delegate::DomainGuilt::kUnknown);
  }
}

void GpuHostImpl::ConnectFrameSinkManager(
    mojom::FrameSinkManagerRequest request,
    mojom::FrameSinkManagerClientPtrInfo client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::ConnectFrameSinkManager");

  mojom::FrameSinkManagerParamsPtr params =
      mojom::FrameSinkManagerParams::New();
  params->restart_id = params_.restart_id;
  params->use_activation_deadline =
      params_.deadline_to_synchronize_surfaces.has_value();
  params->activation_deadline_in_frames =
      params_.deadline_to_synchronize_surfaces.value_or(0u);
  params->frame_sink_manager = std::move(request);
  params->frame_sink_manager_client = std::move(client);
  viz_main_ptr_->CreateFrameSinkManager(std::move(params));
}

void GpuHostImpl::EstablishGpuChannel(int client_id,
                                      uint64_t client_tracing_id,
                                      bool is_gpu_host,
                                      EstablishChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!delegate_->GpuAccessAllowed()) {
    DVLOG(1) << "GPU blacklisted, refusing to open a GPU channel.";
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  if (gpu::IsReservedClientId(client_id)) {
    // The display-compositor/GrShaderCache in the gpu process uses these
    // special client ids.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  bool cache_shaders_on_disk =
      delegate_->GetShaderCacheFactory()->Get(client_id) != nullptr;

  channel_requests_.push(std::move(callback));
  gpu_service_ptr_->EstablishGpuChannel(
      client_id, client_tracing_id, is_gpu_host, cache_shaders_on_disk,
      base::BindOnce(&GpuHostImpl::OnChannelEstablished,
                     weak_ptr_factory_.GetWeakPtr(), client_id));

  if (!params_.disable_gpu_shader_disk_cache) {
    CreateChannelCache(client_id);

    bool oopd_enabled =
        base::FeatureList::IsEnabled(features::kVizDisplayCompositor);
    if (oopd_enabled)
      CreateChannelCache(gpu::kInProcessCommandBufferClientId);

    bool oopr_enabled =
        base::FeatureList::IsEnabled(features::kDefaultEnableOopRasterization);
    if (oopr_enabled)
      CreateChannelCache(gpu::kGrShaderCacheClientId);
  }
}

void GpuHostImpl::SendOutstandingReplies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& handler : connection_error_handlers_)
    std::move(handler).Run();
  connection_error_handlers_.clear();

  // Send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    auto callback = std::move(channel_requests_.front());
    channel_requests_.pop();
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuHostInvalid);
  }
}

mojom::GpuService* GpuHostImpl::gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(gpu_service_ptr_.is_bound());
  return gpu_service_ptr_.get();
}

std::string GpuHostImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo& info = delegate_->GetGPUInfo();
    const gpu::GPUInfo::GPUDevice& active_gpu = info.active_gpu();

    shader_prefix_key_ = params_.product + "-" + info.gl_vendor + "-" +
                         info.gl_renderer + "-" + active_gpu.driver_version +
                         "-" + active_gpu.driver_vendor;

#if defined(OS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    // TODO(ericrk): Remove this after it's up for a few days.
    // https://crbug.com/699122.
    CHECK(!build_fp.empty());
    shader_prefix_key_ += "-" + build_fp;
#endif
  }

  return shader_prefix_key_;
}

void GpuHostImpl::LoadedShader(int32_t client_id,
                               const std::string& key,
                               const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string prefix = GetShaderPrefixKey();
  bool prefix_ok = !key.compare(0, prefix.length(), prefix);
  UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
  if (prefix_ok) {
    // Remove the prefix from the key before load.
    std::string key_no_prefix = key.substr(prefix.length() + 1);
    gpu_service_ptr_->LoadedShader(client_id, key_no_prefix, data);
  }
}

void GpuHostImpl::CreateChannelCache(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::CreateChannelCache");

  scoped_refptr<gpu::ShaderDiskCache> cache =
      delegate_->GetShaderCacheFactory()->Get(client_id);
  if (!cache)
    return;

  cache->set_shader_loaded_callback(base::BindRepeating(
      &GpuHostImpl::LoadedShader, weak_ptr_factory_.GetWeakPtr(), client_id));

  client_id_to_shader_cache_[client_id] = cache;
}

void GpuHostImpl::OnChannelEstablished(
    int client_id,
    mojo::ScopedMessagePipeHandle channel_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::OnChannelEstablished");

  DCHECK(!channel_requests_.empty());
  auto callback = std::move(channel_requests_.front());
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (channel_handle.is_valid() && !delegate_->GpuAccessAllowed()) {
    gpu_service_ptr_->CloseChannel(client_id);
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    RecordLogMessage(logging::LOG_WARNING, "WARNING",
                     "Hardware acceleration is unavailable.");
    return;
  }

  std::move(callback).Run(std::move(channel_handle), delegate_->GetGPUInfo(),
                          delegate_->GetGpuFeatureInfo(),
                          EstablishChannelStatus::kSuccess);
}

void GpuHostImpl::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", true);
  initialized_ = true;

  // Set GPU driver bug workaround flags that are checked on the browser side.
  wake_up_gpu_before_drawing_ =
      gpu_feature_info.IsWorkaroundEnabled(gpu::WAKE_UP_GPU_BEFORE_DRAWING);
  dont_disable_webgl_when_compositor_context_lost_ =
      gpu_feature_info.IsWorkaroundEnabled(
          gpu::DONT_DISABLE_WEBGL_WHEN_COMPOSITOR_CONTEXT_LOST);

  delegate_->UpdateGpuInfo(gpu_info, gpu_feature_info,
                           gpu_info_for_hardware_gpu,
                           gpu_feature_info_for_hardware_gpu);
}

void GpuHostImpl::DidFailInitialize() {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", false);
  delegate_->DidFailInitialize();
}

void GpuHostImpl::DidCreateContextSuccessfully() {
  delegate_->DidCreateContextSuccessfully();
}

void GpuHostImpl::DidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuHostImpl::DidDestroyOffscreenContext(const GURL& url) {
  // We only want to remove *one* of the entries in the multiset for this
  // particular URL, so can't use the erase method taking a key.
  auto candidate = urls_with_live_offscreen_contexts_.find(url);
  if (candidate != urls_with_live_offscreen_contexts_.end())
    urls_with_live_offscreen_contexts_.erase(candidate);
}

void GpuHostImpl::DidDestroyChannel(int32_t client_id) {
  TRACE_EVENT0("gpu", "GpuHostImpl::DidDestroyChannel");
  client_id_to_shader_cache_.erase(client_id);
}

void GpuHostImpl::DidLoseContext(bool offscreen,
                                 gpu::error::ContextLostReason reason,
                                 const GURL& active_url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuHostImpl::DidLoseContext", "reason", reason, "url",
               active_url.possibly_invalid_spec());

  if (!offscreen || active_url.is_empty()) {
    // Assume that the loss of the compositor's or accelerated canvas'
    // context is a serious event and blame the loss on all live
    // offscreen contexts. This more robustly handles situations where
    // the GPU process may not actually detect the context loss in the
    // offscreen context. However, situations have been seen where the
    // compositor's context can be lost due to driver bugs (as of this
    // writing, on Android), so allow that possibility.
    if (!dont_disable_webgl_when_compositor_context_lost_)
      BlockLiveOffscreenContexts();
    return;
  }

  Delegate::DomainGuilt guilt = Delegate::DomainGuilt::kUnknown;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = Delegate::DomainGuilt::kKnown;
      break;
    // Treat most other error codes as though they had unknown provenance.
    // In practice this doesn't affect the user experience. A lost context
    // of either known or unknown guilt still causes user-level 3D APIs
    // (e.g. WebGL) to be blocked on that domain until the user manually
    // reenables them.
    case gpu::error::kUnknown:
    case gpu::error::kOutOfMemory:
    case gpu::error::kMakeCurrentFailed:
    case gpu::error::kGpuChannelLost:
    case gpu::error::kInvalidGpuMessage:
      break;
    case gpu::error::kInnocent:
      return;
  }

  delegate_->BlockDomainFrom3DAPIs(active_url, guilt);
}

void GpuHostImpl::DisableGpuCompositing() {
  delegate_->DisableGpuCompositing();
}

void GpuHostImpl::SetChildSurface(gpu::SurfaceHandle parent,
                                  gpu::SurfaceHandle child) {
#if defined(OS_WIN)
  constexpr char kBadMessageError[] = "Bad parenting request from gpu process.";
  if (!params_.in_process) {
    DWORD parent_process_id = 0;
    DWORD parent_thread_id =
        ::GetWindowThreadProcessId(parent, &parent_process_id);
    if (!parent_thread_id || parent_process_id != ::GetCurrentProcessId()) {
      LOG(ERROR) << kBadMessageError;
      return;
    }

    DWORD child_process_id = 0;
    DWORD child_thread_id =
        ::GetWindowThreadProcessId(child, &child_process_id);
    if (!child_thread_id || child_process_id != pid_) {
      LOG(ERROR) << kBadMessageError;
      return;
    }
  }

  if (!gfx::RenderingWindowManager::GetInstance()->RegisterChild(parent,
                                                                 child)) {
    LOG(ERROR) << kBadMessageError;
  }
#else
  NOTREACHED();
#endif
}

void GpuHostImpl::StoreShaderToDisk(int32_t client_id,
                                    const std::string& key,
                                    const std::string& shader) {
  TRACE_EVENT0("gpu", "GpuHostImpl::StoreShaderToDisk");
  auto iter = client_id_to_shader_cache_.find(client_id);
  // If the cache doesn't exist then this is an off the record profile.
  if (iter == client_id_to_shader_cache_.end())
    return;
  std::string prefix = GetShaderPrefixKey();
  iter->second->Cache(prefix + ":" + key, shader);
}

void GpuHostImpl::RecordLogMessage(int32_t severity,
                                   const std::string& header,
                                   const std::string& message) {
  delegate_->RecordLogMessage(severity, header, message);
}

}  // namespace viz
