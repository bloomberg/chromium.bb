// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_
#define CONTENT_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_

#include <vector>

#include "media/video/video_decode_context.h"

class MessageLoop;

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
// 1. Memory mapped textures (aka software decoding mode).
//    Each texture is memory mapped and appears to the VideoDecodeEngine as
//    system memory.
//
//    The usage of the textures is that the VideoDecodeEngine is performing
//    software video decoding and use them as if they are allocated in plain
//    system memory (in fact they are allocated in system memory and shared
//    bwith the GPU process). An additional step of uploading the content to
//    video memory is needed. Since VideoDecodeEngine is unaware of the video
//    memory, this upload operation is performed by calling
//    ConvertToVideoFrame().
//
//    After the content is uploaded to video memory, WebKit will see the video
//    frame as textures and will perform the necessary operations for
//    rendering.
//
// 2. Opaque textures (aka hardware decoding mode).
//    In this mode of operation each video frame is backed by some opaque
//    textures. This is used only when hardware video decoding is used. The
//    textures needs to be generated and allocated inside the renderer process
//    first. This will establish a translation between texture ID in the
//    renderer process and the GPU process.
//
//    The texture ID generated is used by IpcVideoDecodeEngine only to be sent
//    the GPU process. Inside the GPU process the texture ID is translated to
//    a real texture ID inside the actual context. The real texture ID is then
//    assigned to the hardware video decoder for storing the video frame.
//
//    WebKit will see the video frame as a normal textures and perform
//    necessary render operations.
//
// In both operation modes, the objective is to have WebKit see the video frames
// as regular textures.
//
// THREAD SEMANTICS
//
// All methods of this class can be called on any thread. GLES2 context and all
// OpenGL method calls are accessed on the Render Thread. As as result all Tasks
// given to this object are executed on the Render Thread.
//
// Since this class is not refcounted, it is important to destroy objects of
// this class only when the Task given to Destroy() is called.
//
class Gles2VideoDecodeContext : public media::VideoDecodeContext {
 public:
  // |message_loop| is the message of the Render Thread.
  // |memory_mapped| determines if textures allocated are memory mapped.
  // |context| is the graphics context for generating textures.
  Gles2VideoDecodeContext(MessageLoop* message_loop,
                          bool memory_mapped, ggl::Context* context);
  virtual ~Gles2VideoDecodeContext();

  // media::VideoDecodeContext implementation.
  virtual void* GetDevice();
  virtual void AllocateVideoFrames(
      int frames_num, size_t width, size_t height,
      media::VideoFrame::Format format,
      std::vector<scoped_refptr<media::VideoFrame> >* frames_out, Task* task);
  virtual void ReleaseAllVideoFrames();
  virtual void ConvertToVideoFrame(void* buffer,
                                   scoped_refptr<media::VideoFrame> frame,
                                   Task* task);
  virtual void Destroy(Task* task);

  // Accessor of the current mode of this decode context.
  bool IsMemoryMapped() const { return memory_mapped_; }

 private:
  // Message loop for Render Thread.
  MessageLoop* message_loop_;

  // Type of storage provided by this class.
  bool memory_mapped_;

  // Pointer to the GLES2 context.
  ggl::Context* context_;

  // VideoFrames allocated.
  std::vector<scoped_refptr<media::VideoFrame> > frames_;

  DISALLOW_COPY_AND_ASSIGN(Gles2VideoDecodeContext);
};

#endif  // CONTENT_RENDERER_MEDIA_GLES2_VIDEO_DECODE_CONTEXT_H_
