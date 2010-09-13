// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_
#define CHROME_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_

#include "base/message_loop.h"
#include "media/video/video_decode_context.h"

namespace ggl {
class Context;
}  // namespace ggl

// FUNCTIONS
//
// This is a class that provides a video decode context using a ggl::Context
// backend.
//
// It provides resources for a VideoDecodeEngine to store decoded video frames.
//
// This class is aware of the command buffer implementation of GLES2 inside the
// Chrome renderer and keeps a reference of ggl::Context. It might use GLES2
// commands specific to Chrome's renderer process to provide needed resources.
//
// There are two different kinds of video frame storage provided by this class:
// 1. Memory mapped YUV textures (aka software decoding mode).
//    Each video frame allocated is backed by 3 luminance textures carrying
//    the Y, U and V planes.
//
//    Furthermore each texture is memory mapped and appears to the
//    VideoDecodeEngine as 3 planes backed by system memory.
//
//    The usage of these 3 textures is that the VideoDecodeEngine is performing
//    software video decoding and use them as if they are allocated in plain
//    system memory (in fact they are allocated in system memory and shared
//    bwith the GPU process). An additional step of uploading the content to
//    video memory is needed. Since VideoDecodeEngine is unaware of the video
//    memory, this upload operation is performed by video renderer provided by
//    Chrome.
//
//    After the content is uploaded to video memory, WebKit will see the video
//    frame as 3 textures and will perform the necessary operations for
//    rendering.
//
// 2. RGBA texture (aka hardware decoding mode).
//    In this mode of operation each video frame is backed by a RGBA texture.
//    This is used only when hardware video decoding is used. The texture needs
//    to be generated and allocated inside the renderer process first. This
//    will establish a translation between texture ID in the renderer process
//    and the GPU process.
//
//    The texture ID generated is used by IpcVideoDecodeEngine only to be sent
//    the GPU process. Inside the GPU process the texture ID is translated to
//    a real texture ID inside the actual context. The real texture ID is then
//    assigned to the hardware video decoder for storing the video frame.
//
//    WebKit will see the video frame as a normal RGBA texture and perform
//    necessary render operations.
//
// In both operation modes, the objective is to have WebKit see the video frames
// as regular textures.
//
// THREAD SEMANTICS
//
// This class is accessed on two threads, namely the Render Thread and the
// Video Decoder Thread.
//
// GLES2 context and all OpenGL method calls should be accessed on the Render
// Thread.
//
// VideoDecodeContext implementations are accessed on the Video Decoder Thread.
//
class Gles2VideoDecodeContext : public media::VideoDecodeContext {
 public:
  enum StorageType {
    // This video decode context provides YUV textures as storage. This is used
    // only in software decoding mode.
    kMemoryMappedYuvTextures,

    // This video decode context provides RBGA textures as storage. This is
    // used in hardware decoding mode.
    kRgbaTextures,
  };

  //--------------------------------------------------------------------------
  // Render Thread
  Gles2VideoDecodeContext(StorageType type, ggl::Context* context);

  // TODO(hclam): Need to figure out which thread destroys this object.
  virtual ~Gles2VideoDecodeContext();

  //--------------------------------------------------------------------------
  // Video Decoder Thread

  // media::VideoDecodeContext implementation.
  virtual void* GetDevice();
  virtual void AllocateVideoFrames(int n, size_t width, size_t height,
                                   AllocationCompleteCallback* callback);
  virtual void ReleaseVideoFrames(int n, media::VideoFrame* frames);
  virtual void Destroy(DestructionCompleteCallback* callback);

  //--------------------------------------------------------------------------
  // Any thread
  // Accessor of the current mode of this decode context.
  bool IsMemoryMapped() const { return type_ == kMemoryMappedYuvTextures; }

 private:
  // Message loop that this object lives on. This is the message loop that
  // this object is created.
  MessageLoop* message_loop_;

  // Type of storage provided by this class.
  StorageType type_;

  // Pointer to the GLES2 context.
  ggl::Context* context_;

  DISALLOW_COPY_AND_ASSIGN(Gles2VideoDecodeContext);
};

#endif  // CHROME_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_
