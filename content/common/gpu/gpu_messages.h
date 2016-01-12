// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/common/gpu/gpu_stream_priority.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/gpu_command_buffer_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_types.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gpu_preference.h"

#if defined(OS_ANDROID)
#include "content/common/android/surface_texture_peer.h"
#elif defined(OS_MACOSX)
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/mac/io_surface.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START GpuMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::CauseForGpuLaunch,
                          content::CAUSE_FOR_GPU_LAUNCH_MAX_ENUM - 1)
IPC_ENUM_TRAITS_MAX_VALUE(content::CreateCommandBufferResult,
                          content::CREATE_COMMAND_BUFFER_RESULT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::GpuPreference,
                          gfx::GpuPreferenceLast)
IPC_ENUM_TRAITS_MAX_VALUE(content::GpuStreamPriority,
                          content::GpuStreamPriority::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::SurfaceType,
                          gfx::SURFACE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::SwapResult, gfx::SwapResult::SWAP_RESULT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::MemoryAllocation::PriorityCutoff,
                          gpu::MemoryAllocation::CUTOFF_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::error::ContextLostReason,
                          gpu::error::kContextLostReasonLast)
IPC_ENUM_TRAITS_MAX_VALUE(media::JpegDecodeAccelerator::Error,
                          media::JpegDecodeAccelerator::LARGEST_ERROR_ENUM)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoEncodeAccelerator::Error,
                          media::VideoEncodeAccelerator::kErrorMax)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(gpu::CollectInfoResult,
                              gpu::kCollectInfoNone,
                              gpu::kCollectInfoFatalFailure)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(gpu::VideoCodecProfile,
                              gpu::VIDEO_CODEC_PROFILE_MIN,
                              gpu::VIDEO_CODEC_PROFILE_MAX)

IPC_STRUCT_BEGIN(GPUCreateCommandBufferConfig)
  IPC_STRUCT_MEMBER(int32_t, share_group_id)
  IPC_STRUCT_MEMBER(int32_t, stream_id)
  IPC_STRUCT_MEMBER(content::GpuStreamPriority, stream_priority)
  IPC_STRUCT_MEMBER(std::vector<int>, attribs)
  IPC_STRUCT_MEMBER(GURL, active_url)
  IPC_STRUCT_MEMBER(gfx::GpuPreference, gpu_preference)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuMsg_EstablishChannel_Params)
  IPC_STRUCT_MEMBER(int, client_id)
  IPC_STRUCT_MEMBER(uint64_t, client_tracing_id)
  IPC_STRUCT_MEMBER(bool, preempts)
  IPC_STRUCT_MEMBER(bool, preempted)
  IPC_STRUCT_MEMBER(bool, allow_future_sync_points)
  IPC_STRUCT_MEMBER(bool, allow_real_time_streams)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuMsg_CreateGpuMemoryBuffer_Params)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferId, id)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(gfx::BufferUsage, usage)
  IPC_STRUCT_MEMBER(int32_t, client_id)
  IPC_STRUCT_MEMBER(gfx::PluginWindowHandle, surface_handle)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuMsg_CreateGpuMemoryBufferFromHandle_Params)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, handle)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferId, id)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(int32_t, client_id)
IPC_STRUCT_END()

#if defined(OS_MACOSX)
IPC_STRUCT_BEGIN(GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params)
  IPC_STRUCT_MEMBER(int32_t, surface_id)
  // Only one of ca_context_id or io_surface may be non-0.
  IPC_STRUCT_MEMBER(CAContextID, ca_context_id)
  IPC_STRUCT_MEMBER(gfx::ScopedRefCountedIOSurfaceMachPort, io_surface)
  IPC_STRUCT_MEMBER(int32_t, route_id)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(float, scale_factor)
  IPC_STRUCT_MEMBER(std::vector<ui::LatencyInfo>, latency_info)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AcceleratedSurfaceMsg_BufferPresented_Params)
  // The vsync parameters, to synchronize presentation with the display.
  IPC_STRUCT_MEMBER(base::TimeTicks, vsync_timebase)
  IPC_STRUCT_MEMBER(base::TimeDelta, vsync_interval)
IPC_STRUCT_END()
#endif

IPC_STRUCT_BEGIN(AcceleratedJpegDecoderMsg_Decode_Params)
  IPC_STRUCT_MEMBER(int32_t, input_buffer_id)
  IPC_STRUCT_MEMBER(gfx::Size, coded_size)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, input_buffer_handle)
  IPC_STRUCT_MEMBER(uint32_t, input_buffer_size)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, output_video_frame_handle)
  IPC_STRUCT_MEMBER(uint32_t, output_buffer_size)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AcceleratedVideoDecoderMsg_Decode_Params)
  IPC_STRUCT_MEMBER(int32_t, bitstream_buffer_id)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, buffer_handle)
  IPC_STRUCT_MEMBER(uint32_t, size)
  IPC_STRUCT_MEMBER(base::TimeDelta, presentation_timestamp)
  IPC_STRUCT_MEMBER(std::string, key_id)
  IPC_STRUCT_MEMBER(std::string, iv)
  IPC_STRUCT_MEMBER(std::vector<media::SubsampleEntry>, subsamples)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AcceleratedVideoEncoderMsg_Encode_Params)
  IPC_STRUCT_MEMBER(int32_t, frame_id)
  IPC_STRUCT_MEMBER(base::TimeDelta, timestamp)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, buffer_handle)
  IPC_STRUCT_MEMBER(uint32_t, buffer_offset)
  IPC_STRUCT_MEMBER(uint32_t, buffer_size)
  IPC_STRUCT_MEMBER(bool, force_keyframe)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AcceleratedVideoEncoderMsg_Encode_Params2)
  IPC_STRUCT_MEMBER(int32_t, frame_id)
  IPC_STRUCT_MEMBER(base::TimeDelta, timestamp)
  IPC_STRUCT_MEMBER(std::vector<gfx::GpuMemoryBufferHandle>,
                    gpu_memory_buffer_handles)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(bool, force_keyframe)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GPUCommandBufferConsoleMessage)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(std::string, message)
IPC_STRUCT_END()

#if defined(OS_ANDROID)
IPC_STRUCT_BEGIN(GpuStreamTextureMsg_MatrixChanged_Params)
  IPC_STRUCT_MEMBER(float, m00)
  IPC_STRUCT_MEMBER(float, m01)
  IPC_STRUCT_MEMBER(float, m02)
  IPC_STRUCT_MEMBER(float, m03)
  IPC_STRUCT_MEMBER(float, m10)
  IPC_STRUCT_MEMBER(float, m11)
  IPC_STRUCT_MEMBER(float, m12)
  IPC_STRUCT_MEMBER(float, m13)
  IPC_STRUCT_MEMBER(float, m20)
  IPC_STRUCT_MEMBER(float, m21)
  IPC_STRUCT_MEMBER(float, m22)
  IPC_STRUCT_MEMBER(float, m23)
  IPC_STRUCT_MEMBER(float, m30)
  IPC_STRUCT_MEMBER(float, m31)
  IPC_STRUCT_MEMBER(float, m32)
  IPC_STRUCT_MEMBER(float, m33)
IPC_STRUCT_END()
#endif

IPC_STRUCT_BEGIN(GpuCommandBufferMsg_CreateImage_Params)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, gpu_memory_buffer)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(uint32_t, internal_format)
  IPC_STRUCT_MEMBER(uint64_t, image_release_count)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::DxDiagNode)
  IPC_STRUCT_TRAITS_MEMBER(values)
  IPC_STRUCT_TRAITS_MEMBER(children)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GPUInfo::GPUDevice)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(device_id)
  IPC_STRUCT_TRAITS_MEMBER(active)
  IPC_STRUCT_TRAITS_MEMBER(vendor_string)
  IPC_STRUCT_TRAITS_MEMBER(device_string)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::VideoDecodeAccelerator::Config)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(is_encrypted)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoDecodeAcceleratorSupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(min_resolution)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoDecodeAcceleratorCapabilities)
  IPC_STRUCT_TRAITS_MEMBER(supported_profiles)
  IPC_STRUCT_TRAITS_MEMBER(flags)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoEncodeAcceleratorSupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate_numerator)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate_denominator)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GPUInfo)
  IPC_STRUCT_TRAITS_MEMBER(initialization_time)
  IPC_STRUCT_TRAITS_MEMBER(optimus)
  IPC_STRUCT_TRAITS_MEMBER(amd_switchable)
  IPC_STRUCT_TRAITS_MEMBER(lenovo_dcute)
  IPC_STRUCT_TRAITS_MEMBER(gpu)
  IPC_STRUCT_TRAITS_MEMBER(secondary_gpus)
  IPC_STRUCT_TRAITS_MEMBER(adapter_luid)
  IPC_STRUCT_TRAITS_MEMBER(driver_vendor)
  IPC_STRUCT_TRAITS_MEMBER(driver_version)
  IPC_STRUCT_TRAITS_MEMBER(driver_date)
  IPC_STRUCT_TRAITS_MEMBER(pixel_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(max_msaa_samples)
  IPC_STRUCT_TRAITS_MEMBER(machine_model_name)
  IPC_STRUCT_TRAITS_MEMBER(machine_model_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_vendor)
  IPC_STRUCT_TRAITS_MEMBER(gl_renderer)
  IPC_STRUCT_TRAITS_MEMBER(gl_extensions)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_vendor)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_extensions)
  IPC_STRUCT_TRAITS_MEMBER(gl_reset_notification_strategy)
  IPC_STRUCT_TRAITS_MEMBER(can_lose_context)
  IPC_STRUCT_TRAITS_MEMBER(software_rendering)
  IPC_STRUCT_TRAITS_MEMBER(direct_rendering)
  IPC_STRUCT_TRAITS_MEMBER(sandboxed)
  IPC_STRUCT_TRAITS_MEMBER(process_crash_count)
  IPC_STRUCT_TRAITS_MEMBER(in_process_gpu)
  IPC_STRUCT_TRAITS_MEMBER(basic_info_state)
  IPC_STRUCT_TRAITS_MEMBER(context_info_state)
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(dx_diagnostics_info_state)
  IPC_STRUCT_TRAITS_MEMBER(dx_diagnostics)
#endif
  IPC_STRUCT_TRAITS_MEMBER(video_decode_accelerator_capabilities)
  IPC_STRUCT_TRAITS_MEMBER(video_encode_accelerator_supported_profiles)
  IPC_STRUCT_TRAITS_MEMBER(jpeg_decode_accelerator_supported)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GPUVideoMemoryUsageStats::ProcessStats)
  IPC_STRUCT_TRAITS_MEMBER(video_memory)
  IPC_STRUCT_TRAITS_MEMBER(has_duplicates)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GPUVideoMemoryUsageStats)
  IPC_STRUCT_TRAITS_MEMBER(process_map)
  IPC_STRUCT_TRAITS_MEMBER(bytes_allocated)
  IPC_STRUCT_TRAITS_MEMBER(bytes_allocated_historical_max)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GPUMemoryUmaStats)
  IPC_STRUCT_TRAITS_MEMBER(bytes_allocated_current)
  IPC_STRUCT_TRAITS_MEMBER(bytes_allocated_max)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::MemoryAllocation)
  IPC_STRUCT_TRAITS_MEMBER(bytes_limit_when_visible)
  IPC_STRUCT_TRAITS_MEMBER(priority_cutoff_when_visible)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GLSurfaceHandle)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(transport_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Tells the GPU process to initialize itself. The browser explicitly
// requests this be done so that we are guaranteed that the channel is set
// up between the browser and GPU process before doing any work that might
// potentially crash the GPU process. Detection of the child process
// exiting abruptly is predicated on having the IPC channel set up.
IPC_MESSAGE_CONTROL0(GpuMsg_Initialize)

// Tells the GPU process to shutdown itself.
IPC_MESSAGE_CONTROL0(GpuMsg_Finalize)

// Tells the GPU process to create a new channel for communication with a
// given client.  The channel name is returned in a
// GpuHostMsg_ChannelEstablished message.  The client ID is passed so that
// the GPU process reuses an existing channel to that process if it exists.
// This ID is a unique opaque identifier generated by the browser process.
// The client_tracing_id is a unique ID used for the purposes of tracing.
IPC_MESSAGE_CONTROL1(GpuMsg_EstablishChannel,
                     GpuMsg_EstablishChannel_Params /* params */)

// Tells the GPU process to close the channel identified by IPC channel
// handle.  If no channel can be identified, do nothing.
IPC_MESSAGE_CONTROL1(GpuMsg_CloseChannel,
                     IPC::ChannelHandle /* channel_handle */)

// Tells the GPU process to create a new command buffer that renders directly
// to a native view. A corresponding GpuCommandBufferStub is created.
IPC_MESSAGE_CONTROL4(GpuMsg_CreateViewCommandBuffer,
                     gfx::GLSurfaceHandle,         /* compositing_surface */
                     int32_t,                      /* client_id */
                     GPUCreateCommandBufferConfig, /* init_params */
                     int32_t /* route_id */)

// Tells the GPU process to create a new gpu memory buffer.
IPC_MESSAGE_CONTROL1(GpuMsg_CreateGpuMemoryBuffer,
                     GpuMsg_CreateGpuMemoryBuffer_Params)

// Tells the GPU process to create a new gpu memory buffer from an existing
// handle.
IPC_MESSAGE_CONTROL1(GpuMsg_CreateGpuMemoryBufferFromHandle,
                     GpuMsg_CreateGpuMemoryBufferFromHandle_Params)

// Tells the GPU process to destroy buffer.
IPC_MESSAGE_CONTROL3(GpuMsg_DestroyGpuMemoryBuffer,
                     gfx::GpuMemoryBufferId, /* id */
                     int32_t,                /* client_id */
                     gpu::SyncToken /* sync_token */)

// Create and initialize a hardware jpeg decoder using the specified route_id.
// Created decoders should be freed with AcceleratedJpegDecoderMsg_Destroy when
// no longer needed.
IPC_SYNC_MESSAGE_CONTROL1_1(GpuMsg_CreateJpegDecoder,
                            int32_t /* route_id */,
                            bool /* succeeded */)

// Tells the GPU process to create a context for collecting graphics card
// information.
IPC_MESSAGE_CONTROL0(GpuMsg_CollectGraphicsInfo)

// Tells the GPU process to report video_memory information for the task manager
IPC_MESSAGE_CONTROL0(GpuMsg_GetVideoMemoryUsageStats)

#if defined(OS_MACOSX)
// Tells the GPU process that the browser process has handled the swap
// buffers or post sub-buffer request.
IPC_MESSAGE_ROUTED1(AcceleratedSurfaceMsg_BufferPresented,
                    AcceleratedSurfaceMsg_BufferPresented_Params)
#endif

#if defined(OS_ANDROID)
// Tells the GPU process to wake up the GPU because we're about to draw.
IPC_MESSAGE_CONTROL0(GpuMsg_WakeUpGpu)
#endif

// Tells the GPU process to remove all contexts.
IPC_MESSAGE_CONTROL0(GpuMsg_Clean)

// Tells the GPU process to crash.
IPC_MESSAGE_CONTROL0(GpuMsg_Crash)

// Tells the GPU process to hang.
IPC_MESSAGE_CONTROL0(GpuMsg_Hang)

// Tells the GPU process to disable the watchdog thread.
IPC_MESSAGE_CONTROL0(GpuMsg_DisableWatchdog)

// Tells the GPU process that the browser has seen a GPU switch.
IPC_MESSAGE_CONTROL0(GpuMsg_GpuSwitched)

// Sends an input event to the gpu service.
IPC_MESSAGE_CONTROL3(GpuMsg_UpdateValueState,
                     int, /* client_id */
                     unsigned int, /* target */
                     gpu::ValueState /* valuestate */)

//------------------------------------------------------------------------------
// GPU Host Messages
// These are messages to the browser.

// A renderer sends this when it wants to create a connection to the GPU
// process. The browser will create the GPU process if necessary, and will
// return a handle to the channel via a GpuChannelEstablished message.
IPC_SYNC_MESSAGE_CONTROL1_3(GpuHostMsg_EstablishGpuChannel,
                            content::CauseForGpuLaunch,
                            int /* client id */,
                            IPC::ChannelHandle /* handle to channel */,
                            gpu::GPUInfo /* stats about GPU process*/)

// Response from GPU to a GputMsg_Initialize message.
IPC_MESSAGE_CONTROL2(GpuHostMsg_Initialized,
                     bool /* result */,
                     ::gpu::GPUInfo /* gpu_info */)

// Response from GPU to a GpuHostMsg_EstablishChannel message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_ChannelEstablished,
                     IPC::ChannelHandle /* channel_handle */)

// Message from GPU to notify to destroy the channel.
IPC_MESSAGE_CONTROL1(GpuHostMsg_DestroyChannel, int32_t /* client_id */)

// Message to cache the given shader information.
IPC_MESSAGE_CONTROL3(GpuHostMsg_CacheShader,
                     int32_t /* client_id */,
                     std::string /* key */,
                     std::string /* shader */)

// Message to the GPU that a shader was loaded from disk.
IPC_MESSAGE_CONTROL1(GpuMsg_LoadedShader,
                     std::string /* encoded shader */)

// Respond from GPU to a GpuMsg_CreateViewCommandBuffer message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_CommandBufferCreated,
                     content::CreateCommandBufferResult /* result */)

// Response from GPU to a GpuMsg_CreateGpuMemoryBuffer message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GpuMemoryBufferCreated,
                     gfx::GpuMemoryBufferHandle /* handle */)

// Response from GPU to a GpuMsg_CollectGraphicsInfo.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GraphicsInfoCollected,
                     gpu::GPUInfo /* GPU logging stats */)

// Response from GPU to a GpuMsg_GetVideoMemory.
IPC_MESSAGE_CONTROL1(GpuHostMsg_VideoMemoryUsageStats,
                     content::GPUVideoMemoryUsageStats /* GPU memory stats */)

// Message from GPU to add a GPU log message to the about:gpu page.
IPC_MESSAGE_CONTROL3(GpuHostMsg_OnLogMessage,
                     int /*severity*/,
                     std::string /* header */,
                     std::string /* message */)


#if defined(OS_MACOSX)
// Tells the browser that an accelerated surface has swapped.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                     GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params)
#endif

#if defined(OS_WIN)
IPC_MESSAGE_CONTROL2(GpuHostMsg_AcceleratedSurfaceCreatedChildWindow,
                     gfx::PluginWindowHandle /* parent_window */,
                     gfx::PluginWindowHandle /* child_window */)
#endif

IPC_MESSAGE_CONTROL1(GpuHostMsg_DidCreateOffscreenContext,
                     GURL /* url */)

IPC_MESSAGE_CONTROL3(GpuHostMsg_DidLoseContext,
                     bool /* offscreen */,
                     gpu::error::ContextLostReason /* reason */,
                     GURL /* url */)

IPC_MESSAGE_CONTROL1(GpuHostMsg_DidDestroyOffscreenContext,
                     GURL /* url */)

// Tells the browser about GPU memory usage statistics for UMA logging.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GpuMemoryUmaStats,
                     content::GPUMemoryUmaStats /* GPU memory UMA stats */)

// Tells the browser that a context has subscribed to a new target and
// the browser should start sending the corresponding information
IPC_MESSAGE_CONTROL2(GpuHostMsg_AddSubscription,
                     int32_t /* client_id */,
                     unsigned int /* target */)

// Tells the browser that no contexts are subscribed to the target anymore
// so the browser should stop sending the corresponding information
IPC_MESSAGE_CONTROL2(GpuHostMsg_RemoveSubscription,
                     int32_t /* client_id */,
                     unsigned int /* target */)

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

// Tells the GPU process to create a new command buffer that renders to an
// offscreen frame buffer.
IPC_SYNC_MESSAGE_CONTROL3_1(GpuChannelMsg_CreateOffscreenCommandBuffer,
                            gfx::Size,                    /* size */
                            GPUCreateCommandBufferConfig, /* init_params */
                            int32_t,                      /* route_id */
                            bool /* succeeded */)

// The CommandBufferProxy sends this to the GpuCommandBufferStub in its
// destructor, so that the stub deletes the actual CommandBufferService
// object that it's hosting.
IPC_SYNC_MESSAGE_CONTROL1_0(GpuChannelMsg_DestroyCommandBuffer,
                            int32_t /* instance_id */)

// Simple NOP message which can be used as fence to ensure all previous sent
// messages have been received.
IPC_SYNC_MESSAGE_CONTROL0_0(GpuChannelMsg_Nop)

#if defined(OS_ANDROID)
//------------------------------------------------------------------------------
// Stream Texture Messages
// Tells the GPU process create and send the java surface texture object to
// the renderer process through the binder thread.
IPC_MESSAGE_ROUTED2(GpuStreamTextureMsg_EstablishPeer,
                    int32_t, /* primary_id */
                    int32_t /* secondary_id */)

// Tells the GPU process to set the size of StreamTexture from the given
// stream Id.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_SetSize,
                    gfx::Size /* size */)

// Tells the service-side instance to start sending frame available
// notifications.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_StartListening)

// Inform the renderer that a new frame is available.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_FrameAvailable)

// Inform the renderer process that the transform matrix has changed.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_MatrixChanged,
                    GpuStreamTextureMsg_MatrixChanged_Params /* params */)
#endif

//------------------------------------------------------------------------------
// GPU Command Buffer Messages
// These are messages between a renderer process to the GPU process relating to
// a single OpenGL context.
// Initialize a command buffer with the given number of command entries.
// Returns the shared memory handle for the command buffer mapped to the
// calling process.
IPC_SYNC_MESSAGE_ROUTED1_2(GpuCommandBufferMsg_Initialize,
                           base::SharedMemoryHandle /* shared_state */,
                           bool /* result */,
                           gpu::Capabilities /* capabilities */)

// Sets the shared memory buffer used for commands.
IPC_SYNC_MESSAGE_ROUTED1_0(GpuCommandBufferMsg_SetGetBuffer,
                           int32_t /* shm_id */)

// Produces the front buffer into a mailbox. This allows another context to draw
// the output of this context.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ProduceFrontBuffer,
                    gpu::Mailbox /* mailbox */)

// Wait until the token is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_WaitForTokenInRange,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Wait until the get offset is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Asynchronously synchronize the put and get offsets of both processes.
// Caller passes its current put offset. Current state (including get offset)
// is returned in shared memory. The input latency info for the current
// frame is also sent to the GPU process.
IPC_MESSAGE_ROUTED3(GpuCommandBufferMsg_AsyncFlush,
                    int32_t /* put_offset */,
                    uint32_t /* flush_count */,
                    std::vector<ui::LatencyInfo> /* latency_info */)

// Sent by the GPU process to display messages in the console.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ConsoleMsg,
                    GPUCommandBufferConsoleMessage /* msg */)

// Register an existing shared memory transfer buffer. The id that can be
// used to identify the transfer buffer from a command buffer.
IPC_MESSAGE_ROUTED3(GpuCommandBufferMsg_RegisterTransferBuffer,
                    int32_t /* id */,
                    base::SharedMemoryHandle /* transfer_buffer */,
                    uint32_t /* size */)

// Destroy a previously created transfer buffer.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_DestroyTransferBuffer, int32_t /* id */)

// Create and initialize a hardware video decoder using the specified route_id.
// Created decoders should be freed with AcceleratedVideoDecoderMsg_Destroy when
// no longer needed.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_CreateVideoDecoder,
                           media::VideoDecodeAccelerator::Config, /* config */
                           int32_t,                               /* route_id */
                           bool /* succeeded */)

// Create and initialize a hardware video encoder using the specified route_id.
// Created encoders should be freed with AcceleratedVideoEncoderMsg_Destroy when
// no longer needed.
IPC_SYNC_MESSAGE_ROUTED5_1(GpuCommandBufferMsg_CreateVideoEncoder,
                           media::VideoPixelFormat /* input_format */,
                           gfx::Size /* input_visible_size */,
                           media::VideoCodecProfile /* output_profile */,
                           uint32_t /* initial_bitrate */,
                           int32_t, /* route_id */
                           bool /* succeeded */)

// Tells the proxy that there was an error and the command buffer had to be
// destroyed for some reason.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_Destroyed,
                    gpu::error::ContextLostReason, /* reason */
                    gpu::error::Error /* error */)

// Tells the browser that SwapBuffers returned and passes latency info
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SwapBuffersCompleted,
                    std::vector<ui::LatencyInfo> /* latency_info */,
                    gfx::SwapResult /* result */)

// Tells the browser about updated parameters for vsync alignment.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_UpdateVSyncParameters,
                    base::TimeTicks /* timebase */,
                    base::TimeDelta /* interval */)

// Inserts a sync point into the channel. This is handled on the IO thread, so
// can be expected to be reasonably fast, but the sync point is actually
// retired in order with respect to the other calls. The sync point is shared
// across channels.
IPC_SYNC_MESSAGE_ROUTED1_1(GpuCommandBufferMsg_InsertSyncPoint,
                           bool /* retire */,
                           uint32_t /* sync_point */)

// Retires the sync point.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_RetireSyncPoint,
                    uint32_t /* sync_point */)

// Makes this command buffer signal when a sync point is reached, by sending
// back a GpuCommandBufferMsg_SignalSyncPointAck message with the same
// signal_id.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalSyncPoint,
                    uint32_t /* sync_point */,
                    uint32_t /* signal_id */)

IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalSyncToken,
                    gpu::SyncToken /* sync_token */,
                    uint32_t /* signal_id */)

// Makes this command buffer signal when a query is reached, by sending
// back a GpuCommandBufferMsg_SignalSyncPointAck message with the same
// signal_id.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalQuery,
                    uint32_t /* query */,
                    uint32_t /* signal_id */)

// Response to SignalSyncPoint, SignalSyncToken, and SignalQuery.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SignalAck, uint32_t /* signal_id */)

// Create an image from an existing gpu memory buffer. The id that can be
// used to identify the image from a command buffer.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_CreateImage,
                    GpuCommandBufferMsg_CreateImage_Params /* params */)

// Destroy a previously created image.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_DestroyImage, int32_t /* id */)

// Attaches an external image stream to the client texture.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_CreateStreamTexture,
                           uint32_t, /* client_texture_id */
                           int32_t,  /* stream_id */
                           bool /* succeeded */)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Messages
// These messages are sent from Renderer process to GPU process.

// Set a CDM on the decoder to handle encrypted buffers.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_SetCdm,
                    int32_t) /* CDM ID */

// Send input buffer for decoding.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_Decode,
                    AcceleratedVideoDecoderMsg_Decode_Params)

// Give the texture IDs for the textures the decoder will use for output.
IPC_MESSAGE_ROUTED2(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                    std::vector<int32_t>,  /* Picture buffer ID */
                    std::vector<uint32_t>) /* Texture ID */

// Send from Renderer process to the GPU process to recycle the given picture
// buffer for further decoding.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                    int32_t) /* Picture buffer ID */

// Send flush request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Flush)

// Send reset request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Reset)

// Send destroy request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Destroy)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Host Messages
// These messages are sent from GPU process to Renderer process.
// Inform AcceleratedVideoDecoderHost that AcceleratedVideoDecoder has been
// created.

// Notify the CDM setting result.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_CdmAttached,
                    bool) /* success */

// Accelerated video decoder has consumed input buffer from transfer buffer.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                    int32_t) /* Processed buffer ID */

// Allocate video frames for output of the hardware video decoder.
IPC_MESSAGE_ROUTED3(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                    int32_t,   /* Number of video frames to generate */
                    gfx::Size, /* Requested size of buffer */
                    uint32_t)  /* Texture target */

// Decoder reports that a picture is ready and buffer does not need to be passed
// back to the decoder.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer,
                    int32_t) /* Picture buffer ID */

// Decoder reports that a picture is ready.
IPC_MESSAGE_ROUTED4(AcceleratedVideoDecoderHostMsg_PictureReady,
                    int32_t,   /* Picture buffer ID */
                    int32_t,   /* Bitstream buffer ID */
                    gfx::Rect, /* Visible rectangle */
                    bool)      /* Buffer is HW overlay capable */

// Confirm decoder has been flushed.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_FlushDone)

// Confirm decoder has been reset.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_ResetDone)

// Video decoder has encountered an error.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                    uint32_t) /* Error ID */

//------------------------------------------------------------------------------
// Accelerated Video Encoder Messages
// These messages are sent from the Renderer process to GPU process.

// Queue a video frame to the encoder to encode. |frame_id| will be returned
// by AcceleratedVideoEncoderHostMsg_NotifyInputDone.
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderMsg_Encode,
                    AcceleratedVideoEncoderMsg_Encode_Params)

// Queue a GpuMemoryBuffer backed video frame to the encoder to encode.
// |frame_id| will be returned by
// AcceleratedVideoEncoderHostMsg_NotifyInputDone.
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderMsg_Encode2,
                    AcceleratedVideoEncoderMsg_Encode_Params2)

// Queue a buffer to the encoder for use in returning output.  |buffer_id| will
// be returned by AcceleratedVideoEncoderHostMsg_BitstreamBufferReady.
IPC_MESSAGE_ROUTED3(AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer,
                    int32_t /* buffer_id */,
                    base::SharedMemoryHandle /* buffer_handle */,
                    uint32_t /* buffer_size */)

// Request a runtime encoding parameter change.
IPC_MESSAGE_ROUTED2(AcceleratedVideoEncoderMsg_RequestEncodingParametersChange,
                    uint32_t /* bitrate */,
                    uint32_t /* framerate */)

//------------------------------------------------------------------------------
// Accelerated Video Encoder Host Messages
// These messages are sent from GPU process to Renderer process.

// Notify renderer of the input/output buffer requirements of the encoder.
IPC_MESSAGE_ROUTED3(AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers,
                    uint32_t /* input_count */,
                    gfx::Size /* input_coded_size */,
                    uint32_t /* output_buffer_size */)

// Notify the renderer that the encoder has finished using an input buffer.
// There is no congruent entry point in the media::VideoEncodeAccelerator
// interface, in VEA this same done condition is indicated by dropping the
// reference to the media::VideoFrame passed to VEA::Encode().
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderHostMsg_NotifyInputDone,
                    int32_t /* frame_id */)

// Notify the renderer that an output buffer has been filled with encoded data.
IPC_MESSAGE_ROUTED3(AcceleratedVideoEncoderHostMsg_BitstreamBufferReady,
                    int32_t /* bitstream_buffer_id */,
                    uint32_t /* payload_size */,
                    bool /* key_frame */)

// Report error condition.
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderHostMsg_NotifyError,
                    media::VideoEncodeAccelerator::Error /* error */)

// Send destroy request to the encoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoEncoderMsg_Destroy)

//------------------------------------------------------------------------------
// Accelerated JPEG Decoder Messages
// These messages are sent from the Browser process to GPU process.

// Decode one JPEG image from shared memory |input_buffer_handle| with size
// |input_buffer_size|. The input buffer is associated with |input_buffer_id|
// and the size of JPEG image is |coded_size|. Decoded I420 frame data will
// be put onto shared memory associated with |output_video_frame_handle|
// with size limit |output_buffer_size|.
IPC_MESSAGE_ROUTED1(AcceleratedJpegDecoderMsg_Decode,
                    AcceleratedJpegDecoderMsg_Decode_Params)

// Send destroy request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedJpegDecoderMsg_Destroy)

//------------------------------------------------------------------------------
// Accelerated JPEG Decoder Host Messages
// These messages are sent from the GPU process to Browser process.
//
// Report decode status.
IPC_MESSAGE_ROUTED2(AcceleratedJpegDecoderHostMsg_DecodeAck,
                    int32_t, /* bitstream_buffer_id */
                    media::JpegDecodeAccelerator::Error /* error */)

#if defined(OS_CHROMEOS)
//------------------------------------------------------------------------------
// Arc Video Accelerator Messages
// These messages are sent from the Browser process to GPU process.

// Tells the GPU process to create a new channel for communication with
// ArcVideoAccelerator. The channel is returned using
// GpuHostMsg_ArcVideoAcceleratorChannelCreated message.
IPC_MESSAGE_CONTROL0(GpuMsg_CreateArcVideoAcceleratorChannel)

// Tells the GPU process to shutdown arc video service and terminate all
// instances of ArcVideoAccelerator.
IPC_MESSAGE_CONTROL0(GpuMsg_ShutdownArcVideoService)

//------------------------------------------------------------------------------
// Arc Video Accelerator Host Messages
// These messages are sent from the GPU process to Browser process.

// Response from GPU to a GpuMsg_CreateArcVideoAcceleratorChannel message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_ArcVideoAcceleratorChannelCreated,
                     IPC::ChannelHandle /* handle to channel */)
#endif
