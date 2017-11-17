// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <stdint.h>

#include "base/unguessable_token.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/base/overlay_info.h"
#include "media/gpu/ipc/common/media_param_traits.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START MediaMsgStart

IPC_STRUCT_BEGIN(AcceleratedVideoEncoderMsg_Encode_Params)
  IPC_STRUCT_MEMBER(int32_t, frame_id)
  IPC_STRUCT_MEMBER(base::TimeDelta, timestamp)
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, buffer_handle)
  IPC_STRUCT_MEMBER(uint32_t, buffer_offset)
  IPC_STRUCT_MEMBER(uint32_t, buffer_size)
  IPC_STRUCT_MEMBER(bool, force_keyframe)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AcceleratedVideoDecoderHostMsg_PictureReady_Params)
  IPC_STRUCT_MEMBER(int32_t, picture_buffer_id)
  IPC_STRUCT_MEMBER(int32_t, bitstream_buffer_id)
  IPC_STRUCT_MEMBER(gfx::Rect, visible_rect)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(bool, allow_overlay)
  IPC_STRUCT_MEMBER(bool, size_changed)
  IPC_STRUCT_MEMBER(bool, surface_texture)
  IPC_STRUCT_MEMBER(bool, wants_promotion_hint)
IPC_STRUCT_END()

//------------------------------------------------------------------------------
// Utility Messages

// Sent from Renderer to GPU process to request a token identifying the channel.
// These tokens can be used to prove ownership of the channel. The intended use
// case is to share the command buffer with MojoMediaApplication.
IPC_SYNC_MESSAGE_CONTROL0_1(GpuCommandBufferMsg_GetChannelToken,
                            base::UnguessableToken /* channel_token */)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Messages
// These messages are sent from Renderer process to GPU process.

// Create and initialize a hardware video decoder using the specified route_id.
// Created decoders should be freed with AcceleratedVideoDecoderMsg_Destroy when
// no longer needed.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_CreateVideoDecoder,
                           media::VideoDecodeAccelerator::Config /* config */,
                           int32_t /* decoder_route_id */,
                           bool /* succeeded */)

// Send input buffer for decoding.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_Decode, media::BitstreamBuffer)

// Give the texture IDs for the textures the decoder will use for output.
IPC_MESSAGE_ROUTED2(
    AcceleratedVideoDecoderMsg_AssignPictureBuffers,
    std::vector<int32_t>,                          /* Picture buffer ID */
    std::vector<media::PictureBuffer::TextureIds>) /* Texture ID */

// Send from Renderer process to the GPU process to recycle the given picture
// buffer for further decoding.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                    int32_t) /* Picture buffer ID */

// Send flush request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Flush)

// Send reset request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Reset)

// Send overlay info to the decoder.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderMsg_SetOverlayInfo,
                    media::OverlayInfo)

// Send destroy request to the decoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderMsg_Destroy)

//------------------------------------------------------------------------------
// Accelerated Video Decoder Host Messages
// These messages are sent from GPU process to Renderer process.
// Inform AcceleratedVideoDecoderHost that AcceleratedVideoDecoder has been
// created.

// Notify the deferred initialization result.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_InitializationComplete,
                    bool) /* success */

// Accelerated video decoder has consumed input buffer from transfer buffer.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
                    int32_t) /* Processed buffer ID */

// Allocate video frames for output of the hardware video decoder.
IPC_MESSAGE_ROUTED5(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                    int32_t, /* Number of video frames to generate */
                    media::VideoPixelFormat, /* Picture buffer format */
                    uint32_t,                /* Number of textures per frame */
                    gfx::Size,               /* Requested size of buffer */
                    uint32_t)                /* Texture target */

// Decoder reports that a picture is ready and buffer does not need to be passed
// back to the decoder.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer,
                    int32_t) /* Picture buffer ID */

// Decoder reports that a picture is ready.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_PictureReady,
                    AcceleratedVideoDecoderHostMsg_PictureReady_Params)

// Confirm decoder has been flushed.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_FlushDone)

// Confirm decoder has been reset.
IPC_MESSAGE_ROUTED0(AcceleratedVideoDecoderHostMsg_ResetDone)

// Video decoder has encountered an error.
IPC_MESSAGE_ROUTED1(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                    uint32_t /* Error ID */)

//------------------------------------------------------------------------------
// Accelerated Video Encoder Messages
// These messages are sent from the Renderer process to GPU process.

// Create and initialize a hardware video encoder using the specified route_id.
// Created encoders should be freed with AcceleratedVideoEncoderMsg_Destroy when
// no longer needed.
IPC_SYNC_MESSAGE_ROUTED1_1(GpuCommandBufferMsg_CreateVideoEncoder,
                           media::CreateVideoEncoderParams,
                           bool /* succeeded */)

// Queue a video frame to the encoder to encode. |frame_id| will be returned
// by AcceleratedVideoEncoderHostMsg_NotifyInputDone.
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderMsg_Encode,
                    AcceleratedVideoEncoderMsg_Encode_Params)

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
IPC_MESSAGE_ROUTED4(AcceleratedVideoEncoderHostMsg_BitstreamBufferReady,
                    int32_t /* bitstream_buffer_id */,
                    uint32_t /* payload_size */,
                    bool /* key_frame */,
                    base::TimeDelta /* timestamp */)

// Report error condition.
IPC_MESSAGE_ROUTED1(AcceleratedVideoEncoderHostMsg_NotifyError,
                    media::VideoEncodeAccelerator::Error /* error */)

// Send destroy request to the encoder.
IPC_MESSAGE_ROUTED0(AcceleratedVideoEncoderMsg_Destroy)
