// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <string>
#include <vector>

#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/gpu_info.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/gpu_command_buffer_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

#define IPC_MESSAGE_START GpuMsgStart

IPC_STRUCT_BEGIN(GPUCreateCommandBufferConfig)
  IPC_STRUCT_MEMBER(int32, share_group_id)
  IPC_STRUCT_MEMBER(std::string, allowed_extensions)
  IPC_STRUCT_MEMBER(std::vector<int>, attribs)
  IPC_STRUCT_MEMBER(GURL, active_url)
  IPC_STRUCT_MEMBER(gfx::GpuPreference, gpu_preference)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuHostMsg_AcceleratedSurfaceNew_Params)
  IPC_STRUCT_MEMBER(int32, surface_id)
  IPC_STRUCT_MEMBER(int32, width)
  IPC_STRUCT_MEMBER(int32, height)
  IPC_STRUCT_MEMBER(uint64, surface_handle)
  IPC_STRUCT_MEMBER(int32, route_id)
#if defined(OS_MACOSX)
  IPC_STRUCT_MEMBER(gfx::PluginWindowHandle, window)
  IPC_STRUCT_MEMBER(bool, create_transport_dib)
#endif
IPC_STRUCT_END()

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
IPC_STRUCT_BEGIN(GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params)
  IPC_STRUCT_MEMBER(int32, surface_id)
  IPC_STRUCT_MEMBER(uint64, surface_handle)
  IPC_STRUCT_MEMBER(int32, route_id)
#if defined(OS_WIN)
  IPC_STRUCT_MEMBER(gfx::Size, size)
#elif defined(OS_MACOSX)
  IPC_STRUCT_MEMBER(gfx::PluginWindowHandle, window)
#endif
IPC_STRUCT_END()
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

IPC_STRUCT_BEGIN(GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params)
  IPC_STRUCT_MEMBER(int32, surface_id)
  IPC_STRUCT_MEMBER(uint64, surface_handle)
  IPC_STRUCT_MEMBER(int32, route_id)
  IPC_STRUCT_MEMBER(int, x)
  IPC_STRUCT_MEMBER(int, y)
  IPC_STRUCT_MEMBER(int, width)
  IPC_STRUCT_MEMBER(int, height)
#if defined(OS_MACOSX)
  IPC_STRUCT_MEMBER(gfx::PluginWindowHandle, window)
#endif
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuHostMsg_AcceleratedSurfaceRelease_Params)
  IPC_STRUCT_MEMBER(int32, surface_id)
  IPC_STRUCT_MEMBER(uint64, identifier)
  IPC_STRUCT_MEMBER(int32, route_id)
#if defined(OS_MACOSX)
  IPC_STRUCT_MEMBER(gfx::PluginWindowHandle, window)
#endif
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GPUCommandBufferConsoleMessage)
  IPC_STRUCT_MEMBER(int32, id)
  IPC_STRUCT_MEMBER(std::string, message)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::DxDiagNode)
  IPC_STRUCT_TRAITS_MEMBER(values)
  IPC_STRUCT_TRAITS_MEMBER(children)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GpuPerformanceStats)
  IPC_STRUCT_TRAITS_MEMBER(graphics)
  IPC_STRUCT_TRAITS_MEMBER(gaming)
  IPC_STRUCT_TRAITS_MEMBER(overall)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GPUInfo::GPUDevice)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(device_id)
  IPC_STRUCT_TRAITS_MEMBER(vendor_string)
  IPC_STRUCT_TRAITS_MEMBER(device_string)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GPUInfo)
  IPC_STRUCT_TRAITS_MEMBER(finalized)
  IPC_STRUCT_TRAITS_MEMBER(initialization_time)
  IPC_STRUCT_TRAITS_MEMBER(optimus)
  IPC_STRUCT_TRAITS_MEMBER(gpu)
  IPC_STRUCT_TRAITS_MEMBER(secondary_gpus)
  IPC_STRUCT_TRAITS_MEMBER(driver_vendor)
  IPC_STRUCT_TRAITS_MEMBER(driver_version)
  IPC_STRUCT_TRAITS_MEMBER(driver_date)
  IPC_STRUCT_TRAITS_MEMBER(pixel_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_version_string)
  IPC_STRUCT_TRAITS_MEMBER(gl_vendor)
  IPC_STRUCT_TRAITS_MEMBER(gl_renderer)
  IPC_STRUCT_TRAITS_MEMBER(gl_extensions)
  IPC_STRUCT_TRAITS_MEMBER(can_lose_context)
  IPC_STRUCT_TRAITS_MEMBER(gpu_accessible)
  IPC_STRUCT_TRAITS_MEMBER(performance_stats)
  IPC_STRUCT_TRAITS_MEMBER(software_rendering)
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(dx_diagnostics)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(GpuMemoryAllocationForRenderer)
  IPC_STRUCT_TRAITS_MEMBER(gpu_resource_size_in_bytes)
  IPC_STRUCT_TRAITS_MEMBER(suggest_have_backbuffer)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GLSurfaceHandle)
  IPC_STRUCT_TRAITS_MEMBER(handle)
  IPC_STRUCT_TRAITS_MEMBER(transport)
  IPC_STRUCT_TRAITS_MEMBER(parent_gpu_process_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_client_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_context_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_texture_id[0])
  IPC_STRUCT_TRAITS_MEMBER(parent_texture_id[1])
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(content::CauseForGpuLaunch)
IPC_ENUM_TRAITS(gfx::GpuPreference)
IPC_ENUM_TRAITS(gpu::error::ContextLostReason)

IPC_ENUM_TRAITS(media::VideoCodecProfile)

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Tells the GPU process to initialize itself. The browser explicitly
// requests this be done so that we are guaranteed that the channel is set
// up between the browser and GPU process before doing any work that might
// potentially crash the GPU process. Detection of the child process
// exiting abruptly is predicated on having the IPC channel set up.
IPC_MESSAGE_CONTROL0(GpuMsg_Initialize)

// Tells the GPU process to create a new channel for communication with a
// given client.  The channel name is returned in a
// GpuHostMsg_ChannelEstablished message.  The client ID is passed so that
// the GPU process reuses an existing channel to that process if it exists.
// This ID is a unique opaque identifier generated by the browser process.
IPC_MESSAGE_CONTROL2(GpuMsg_EstablishChannel,
                     int /* client_id */,
                     bool /* share_context */)

// Tells the GPU process to close the channel identified by IPC channel
// handle.  If no channel can be identified, do nothing.
IPC_MESSAGE_CONTROL1(GpuMsg_CloseChannel,
                     IPC::ChannelHandle /* channel_handle */)

// Tells the GPU process to create a new command buffer that renders directly
// to a native view. A corresponding GpuCommandBufferStub is created.
IPC_MESSAGE_CONTROL4(GpuMsg_CreateViewCommandBuffer,
                     gfx::GLSurfaceHandle, /* compositing_surface */
                     int32, /* surface_id */
                     int32, /* client_id */
                     GPUCreateCommandBufferConfig /* init_params */)

// Tells the GPU process to create a context for collecting graphics card
// information.
IPC_MESSAGE_CONTROL0(GpuMsg_CollectGraphicsInfo)

// Tells the GPU process that the browser process has finished resizing the
// view.
IPC_MESSAGE_ROUTED0(AcceleratedSurfaceMsg_ResizeViewACK)

// Tells the GPU process that it's safe to start rendering to the surface.
IPC_MESSAGE_ROUTED2(AcceleratedSurfaceMsg_NewACK,
                    uint64 /* surface_handle */,
                    TransportDIB::Handle /* shared memory buffer */)

// Tells the GPU process that the browser process handled the swap
// buffers request.
IPC_MESSAGE_ROUTED0(AcceleratedSurfaceMsg_BuffersSwappedACK)

// Tells the GPU process that the browser process handled the
// PostSubBuffer command.
IPC_MESSAGE_ROUTED0(AcceleratedSurfaceMsg_PostSubBufferACK)

// Tells the GPU process to remove all contexts.
IPC_MESSAGE_CONTROL0(GpuMsg_Clean)

// Tells the GPU process to crash.
IPC_MESSAGE_CONTROL0(GpuMsg_Crash)

// Tells the GPU process to hang.
IPC_MESSAGE_CONTROL0(GpuMsg_Hang)

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
                            content::GPUInfo /* stats about GPU process*/)

// A renderer sends this to the browser process when it wants to
// create a GL context associated with the given view_id.
IPC_SYNC_MESSAGE_CONTROL2_1(GpuHostMsg_CreateViewCommandBuffer,
                            int32, /* surface_id */
                            GPUCreateCommandBufferConfig, /* init_params */
                            int32 /* route_id */)

// Response from GPU to a GpuHostMsg_EstablishChannel message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_ChannelEstablished,
                     IPC::ChannelHandle /* channel_handle */)

// Respond from GPU to a GpuMsg_CreateViewCommandBuffer message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_CommandBufferCreated,
                     int32 /* route_id */)

// Request from GPU to free the browser resources associated with the
// command buffer.
IPC_MESSAGE_CONTROL1(GpuHostMsg_DestroyCommandBuffer,
                     int32 /* surface_id */)

// Response from GPU to a GpuMsg_CollectGraphicsInfo.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GraphicsInfoCollected,
                     content::GPUInfo /* GPU logging stats */)

// Message from GPU to add a GPU log message to the about:gpu page.
IPC_MESSAGE_CONTROL3(GpuHostMsg_OnLogMessage,
                     int /*severity*/,
                     std::string /* header */,
                     std::string /* message */)

// Resize the window that is being drawn into. It's important that this
// resize be synchronized with the swapping of the front and back buffers.
IPC_MESSAGE_CONTROL3(GpuHostMsg_ResizeView,
                     int32 /* surface_id */,
                     int32 /* route_id */,
                     gfx::Size /* size */)

// This message is sent from the GPU process to the browser to notify about a
// new or resized surface in the GPU.  The browser allocates any resources
// needed for it on its end and replies with an ACK containing any shared
// resources/identifiers to be used in the GPU.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfaceNew,
                     GpuHostMsg_AcceleratedSurfaceNew_Params)

// Same as above with a rect of the part of the surface that changed.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                     GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params)

// This message notifies the browser process that the renderer
// swapped a portion of the buffers associated with the given "window", which
// should cause the browser to redraw the compositor's contents.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfacePostSubBuffer,
                     GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params)

// Tells the browser to release whatever resources are associated with
// the given surface. The browser must send an ACK once this operation
// is complete.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfaceRelease,
                     GpuHostMsg_AcceleratedSurfaceRelease_Params)

// Tells the browser to release resources for the given surface until the next
// time swap buffers or post sub buffer is sent.
IPC_MESSAGE_CONTROL1(GpuHostMsg_AcceleratedSurfaceSuspend,
                     int32 /* surface_id */)

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

// Tells the GPU process to create a new command buffer that renders to an
// offscreen frame buffer.
IPC_SYNC_MESSAGE_CONTROL2_1(GpuChannelMsg_CreateOffscreenCommandBuffer,
                            gfx::Size, /* size */
                            GPUCreateCommandBufferConfig, /* init_params */
                            int32 /* route_id */)

// The CommandBufferProxy sends this to the GpuCommandBufferStub in its
// destructor, so that the stub deletes the actual CommandBufferService
// object that it's hosting.
IPC_SYNC_MESSAGE_CONTROL1_0(GpuChannelMsg_DestroyCommandBuffer,
                            int32 /* instance_id */)

// Asks the GPU process whether the creation or destruction of a
// command buffer on the given GPU (integrated or discrete) will cause
// the system to switch which GPU it is using. All contexts that share
// resources need to be created on the same GPU.
IPC_SYNC_MESSAGE_CONTROL2_1(GpuChannelMsg_WillGpuSwitchOccur,
                            bool /* is_creating_context */,
                            gfx::GpuPreference /* preference */,
                            bool /* will_cause_switch */)

// Forcibly closes the channel on the GPU process side, in order to
// have the side effect that all contexts associated with this
// renderer go into the lost state.
IPC_MESSAGE_CONTROL0(GpuChannelMsg_CloseChannel)

//------------------------------------------------------------------------------
// GPU Command Buffer Messages
// These are messages between a renderer process to the GPU process relating to
// a single OpenGL context.
// Initialize a command buffer with the given number of command entries.
// Returns the shared memory handle for the command buffer mapped to the
// calling process.
IPC_SYNC_MESSAGE_ROUTED0_1(GpuCommandBufferMsg_Initialize,
                           bool /* result */)

// Sets the shared memory buffer used to hold the CommandBufferSharedState,
// used to transmit the current state.
IPC_SYNC_MESSAGE_ROUTED1_0(GpuCommandBufferMsg_SetSharedStateBuffer,
                           int32 /* shm_id */)

// Sets the shared memory buffer used for commands.
IPC_SYNC_MESSAGE_ROUTED1_0(GpuCommandBufferMsg_SetGetBuffer,
                           int32 /* shm_id */)

// Sets the parent command buffer. This allows the parent and child to share
// textures.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_SetParent,
                           int32 /* parent_route_id */,
                           uint32 /* parent_texture_id */,
                           bool /* result */)

// Get the current state of the command buffer.
IPC_SYNC_MESSAGE_ROUTED0_1(GpuCommandBufferMsg_GetState,
                           gpu::CommandBuffer::State /* state */)

// Get the current state of the command buffer, as fast as possible.
IPC_SYNC_MESSAGE_ROUTED0_1(GpuCommandBufferMsg_GetStateFast,
                           gpu::CommandBuffer::State /* state */)

// Asynchronously synchronize the put and get offsets of both processes.
// Caller passes its current put offset. Current state (including get offset)
// is returned in shared memory.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_AsyncFlush,
                    int32 /* put_offset */,
                    uint32 /* flush_count */)

// Asynchronously process any commands known to the GPU process. This is only
// used in the event that a channel is unscheduled and needs to be flushed
// again to process any commands issued subsequent to unscheduling. The GPU
// process actually sends it (deferred) to itself.
IPC_MESSAGE_ROUTED0(GpuCommandBufferMsg_Rescheduled)

// Sent by the GPU process to display messages in the console.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ConsoleMsg,
                    GPUCommandBufferConsoleMessage /* msg */)

// Create a shared memory transfer buffer. Returns an id that can be used to
// identify the transfer buffer from a comment.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_CreateTransferBuffer,
                           int32 /* size */,
                           int32 /* id_request (-1 means any) */,
                           int32 /* id */)

// Register an existing shared memory transfer buffer. Returns an id that can be
// used to identify the transfer buffer from a command buffer.
IPC_SYNC_MESSAGE_ROUTED3_1(GpuCommandBufferMsg_RegisterTransferBuffer,
                           base::SharedMemoryHandle /* transfer_buffer */,
                           size_t /* size */,
                           int32 /* id_request (-1 means any) */,
                           int32 /* id */)

// Destroy a previously created transfer buffer.
IPC_SYNC_MESSAGE_ROUTED1_0(GpuCommandBufferMsg_DestroyTransferBuffer,
                           int32 /* id */)

// Get the shared memory handle for a transfer buffer mapped to the callers
// process.
IPC_SYNC_MESSAGE_ROUTED1_2(GpuCommandBufferMsg_GetTransferBuffer,
                           int32 /* id */,
                           base::SharedMemoryHandle /* transfer_buffer */,
                           uint32 /* size */)

// Create and initialize a hardware video decoder, returning its new route_id.
IPC_SYNC_MESSAGE_ROUTED1_1(GpuCommandBufferMsg_CreateVideoDecoder,
                           media::VideoCodecProfile /* profile */,
                           int /* route_id */)

// Release all resources held by the named hardware video decoder.
IPC_SYNC_MESSAGE_ROUTED1_0(GpuCommandBufferMsg_DestroyVideoDecoder,
                           int /* route_id */)

// Send from command buffer stub to proxy when window is invalid and must be
// repainted.
IPC_MESSAGE_ROUTED0(GpuCommandBufferMsg_NotifyRepaint)

// Tells the GPU process to resize an offscreen frame buffer.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ResizeOffscreenFrameBuffer,
                    gfx::Size /* size */)

#if defined(OS_MACOSX)
// On Mac OS X the GPU plugin must be offscreen, because there is no
// true cross-process window hierarchy. For this reason we must send
// resize events explicitly to the command buffer stub so it can
// reallocate its backing store and send the new one back to the
// browser. This message is currently used only on 10.6 and later.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetWindowSize,
                    gfx::Size /* size */)
#endif

// Tells the proxy that there was an error and the command buffer had to be
// destroyed for some reason.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_Destroyed,
                    gpu::error::ContextLostReason /* reason */)

// Request that the GPU process reply with the given message. Reply may be
// delayed.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_Echo,
                    IPC::Message /* reply */)

// Response to a GpuChannelMsg_Echo message.
IPC_MESSAGE_ROUTED0(GpuCommandBufferMsg_EchoAck)

// Send to stub on surface visibility change.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetSurfaceVisible, bool /* visible */)

IPC_MESSAGE_ROUTED0(GpuCommandBufferMsg_DiscardBackbuffer)
IPC_MESSAGE_ROUTED0(GpuCommandBufferMsg_EnsureBackbuffer)

// Sent to proxy when the gpu memory manager changes its memory allocation.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetMemoryAllocation,
                    GpuMemoryAllocationForRenderer /* allocation */)

// Sent to stub when proxy is assigned a memory allocation changed callback.
IPC_MESSAGE_ROUTED1(
    GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback,
    bool /* has_callback */)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Messages
// These messages are sent from Renderer process to GPU process.

// Send input buffer for decoding.
IPC_MESSAGE_ROUTED3(AcceleratedVideoDecoderMsg_Decode,
                    base::SharedMemoryHandle, /* input_buffer_handle */
                    int32, /* bitstream_buffer_id */
                    int32) /* size */

// Sent from Renderer process to the GPU process to give the texture IDs for
// the textures the decoder will use for output.
IPC_MESSAGE_ROUTED3(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                    std::vector<int32>, /* Picture buffer ID */
                    std::vector<uint32>, /* Texture ID */
                    std::vector<gfx::Size>) /* Size */

// Send from Renderer process to the GPU process to recycle the given picture
// buffer for further decoding.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                    int32) /* Picture buffer ID */

// Send flush request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Flush)

// Send reset request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Reset)

// Send destroy request to the decoder.
IPC_SYNC_MESSAGE_ROUTED0_0(AcceleratedVideoDecoderMsg_Destroy)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Host Messages
// These messages are sent from GPU process to Renderer process.
// Inform AcceleratedVideoDecoderHost that AcceleratedVideoDecoder has been
// created.

// Accelerated video decoder has consumed input buffer from transfer buffer.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                    int32) /* Processed buffer ID */

// Allocate video frames for output of the hardware video decoder.
IPC_MESSAGE_ROUTED2(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                    int32, /* Number of video frames to generate */
                    gfx::Size) /* Requested size of buffer */

// Decoder reports that a picture is ready and buffer does not need to be passed
// back to the decoder.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer,
                    int32) /* Picture buffer ID */

// Decoder reports that a picture is ready.
IPC_MESSAGE_ROUTED2(AcceleratedVideoDecoderHostMsg_PictureReady,
                    int32,  /* Picture buffer ID */
                    int32)  /* Bitstream buffer ID */

// Confirm decoder has been flushed.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_FlushDone)

// Confirm decoder has been reset.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_ResetDone)

// Video decoder has encountered an error.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                    uint32) /* Error ID */
