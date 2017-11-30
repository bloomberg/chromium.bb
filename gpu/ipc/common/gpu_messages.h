// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MESSAGES_H_
#define GPU_IPC_COMMON_GPU_MESSAGES_H_

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "gpu/ipc/common/gpu_param_traits.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "url/ipc/url_param_traits.h"

#if defined(OS_MACOSX)
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/mac/io_surface.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

#define IPC_MESSAGE_START GpuChannelMsgStart

IPC_STRUCT_BEGIN(GPUCommandBufferConsoleMessage)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(std::string, message)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GPUCreateCommandBufferConfig)
  IPC_STRUCT_MEMBER(gpu::SurfaceHandle, surface_handle)
  IPC_STRUCT_MEMBER(int32_t, share_group_id)
  IPC_STRUCT_MEMBER(int32_t, stream_id)
  IPC_STRUCT_MEMBER(gpu::SchedulingPriority, stream_priority)
  IPC_STRUCT_MEMBER(gpu::gles2::ContextCreationAttribHelper, attribs)
  IPC_STRUCT_MEMBER(GURL, active_url)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuCommandBufferMsg_CreateImage_Params)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, gpu_memory_buffer)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(uint32_t, internal_format)
  IPC_STRUCT_MEMBER(uint64_t, image_release_count)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuCommandBufferMsg_SwapBuffersCompleted_Params)
#if defined(OS_MACOSX)
  // Mac-specific parameters used to present CALayers hosted in the GPU process.
  // TODO(ccameron): Remove these parameters once the CALayer tree is hosted in
  // the browser process.
  // https://crbug.com/604052
  // Only one of ca_context_id or io_surface may be non-0.
  IPC_STRUCT_MEMBER(CAContextID, ca_context_id)
  IPC_STRUCT_MEMBER(bool, fullscreen_low_power_ca_context_valid)
  IPC_STRUCT_MEMBER(CAContextID, fullscreen_low_power_ca_context_id)
  IPC_STRUCT_MEMBER(gfx::ScopedRefCountedIOSurfaceMachPort, io_surface)
  IPC_STRUCT_MEMBER(gfx::Size, pixel_size)
  IPC_STRUCT_MEMBER(float, scale_factor)
  IPC_STRUCT_MEMBER(gpu::TextureInUseResponses, in_use_responses)
#endif
  IPC_STRUCT_MEMBER(gfx::SwapResponse, response)
IPC_STRUCT_END()

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

// Tells the GPU process to create a new command buffer. A corresponding
// GpuCommandBufferStub is created.  if |surface_handle| is non-null, |size| is
// ignored, and it will render directly to the native surface (only the browser
// process is allowed to create those). Otherwise it will create an offscreen
// backbuffer of dimensions |size|.
IPC_SYNC_MESSAGE_CONTROL3_2(GpuChannelMsg_CreateCommandBuffer,
                            GPUCreateCommandBufferConfig /* init_params */,
                            int32_t /* route_id */,
                            base::SharedMemoryHandle /* shared_state */,
                            gpu::ContextResult,
                            gpu::Capabilities /* capabilities */)

// The CommandBufferProxy sends this to the GpuCommandBufferStub in its
// destructor, so that the stub deletes the actual CommandBufferService
// object that it's hosting.
IPC_SYNC_MESSAGE_CONTROL1_0(GpuChannelMsg_DestroyCommandBuffer,
                            int32_t /* instance_id */)

IPC_MESSAGE_CONTROL1(GpuChannelMsg_FlushCommandBuffers,
                     std::vector<gpu::FlushParams> /* flush_list */)

// Simple NOP message which can be used as fence to ensure all previous sent
// messages have been received.
IPC_SYNC_MESSAGE_CONTROL0_0(GpuChannelMsg_Nop)

#if defined(OS_ANDROID)
//------------------------------------------------------------------------------
// Tells the StreamTexture to send its SurfaceTexture to the browser process,
// via the ScopedSurfaceRequestConduit.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_ForwardForSurfaceRequest,
                    base::UnguessableToken)

// Tells the GPU process to set the size of StreamTexture from the given
// stream Id.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_SetSize, gfx::Size /* size */)

// Tells the service-side instance to start sending frame available
// notifications.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_StartListening)

// Inform the renderer that a new frame is available.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_FrameAvailable)
#endif

//------------------------------------------------------------------------------
// GPU Command Buffer Messages
// These are messages between a renderer process to the GPU process relating to
// a single OpenGL context.

// Sets the shared memory buffer used for commands.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetGetBuffer, int32_t /* shm_id */)

// Takes the front buffer into a mailbox. This allows another context to draw
// the output of this context.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_TakeFrontBuffer,
                    gpu::Mailbox /* mailbox */)

// Returns a front buffer taken with GpuCommandBufferMsg_TakeFrontBuffer. This
// allows it to be reused.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_ReturnFrontBuffer,
                    gpu::Mailbox /* mailbox */,
                    bool /* is_lost */)

// Wait until the token is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_WaitForTokenInRange,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Wait until the get offset is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED3_1(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                           uint32_t /* set_get_buffer_count */,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Asynchronously synchronize the put and get offsets of both processes.
// Caller passes its current put offset. Current state (including get offset)
// is returned in shared memory.
// TODO(sunnyps): This is an internal implementation detail of the gpu service
// and is not sent by the client. Remove this once the non-scheduler code path
// is removed.
IPC_MESSAGE_ROUTED3(GpuCommandBufferMsg_AsyncFlush,
                    int32_t /* put_offset */,
                    uint32_t /* flush_id */,
                    bool /* snapshot_requested */)

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

// Tells the proxy that there was an error and the command buffer had to be
// destroyed for some reason.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_Destroyed,
                    gpu::error::ContextLostReason, /* reason */
                    gpu::error::Error /* error */)

// Tells the browser that SwapBuffers returned.
IPC_MESSAGE_ROUTED1(
    GpuCommandBufferMsg_SwapBuffersCompleted,
    GpuCommandBufferMsg_SwapBuffersCompleted_Params /* params */)

// Tells the browser about updated parameters for vsync alignment.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_UpdateVSyncParameters,
                    base::TimeTicks /* timebase */,
                    base::TimeDelta /* interval */)

// Tells the browser a buffer has been presented on screen.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_BufferPresented,
                    uint64_t, /* swap_id */
                    gfx::PresentationFeedback /* feedback */)

// The receiver will stop processing messages until the Synctoken is signaled.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_WaitSyncToken,
                    gpu::SyncToken /* sync_token */)

// The receiver will asynchronously wait until the SyncToken is signaled, and
// then return a GpuCommandBufferMsg_SignalAck message.
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
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalAck,
                    uint32_t /* signal_id */,
                    gpu::CommandBuffer::State /* state */)

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

// Start or stop VSync sygnal production on GPU side (Windows only).
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetNeedsVSync, bool /* needs_vsync */)

#endif  // GPU_IPC_COMMON_GPU_MESSAGES_H_
