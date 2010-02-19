// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An abstract class to define the behavior of an output buffer sink for
// media::OmxCodec. It is responsible for negotiation of buffer allocation
// for output of media::OmxCodec. It is also responsible for processing buffer
// output from OpenMAX decoder. It is important to note that implementor must
// assure usage of this class is safe on the thread where media::OmxCodec
// lives. Ownership of this object is described in src/media/omx/omx_codec.h.
//
// BUFFER NEGOTIATION
//
// One of the key function of OmxOutputSink is to negotiate output buffer
// allocation with OmxCodec. There are two modes of operation:
//
// 1. Buffer is provided by OmxCodec.
//
// In this case ProvidesEGLImage() will return false and OmxCodec allocates
// output buffers during initialization. Here's a sample sequence of actions:
//
// - OmxCodec allocated buffers during initialization.
// - OmxOutputSink::UseThisBuffer(id, buffer) is called to assign an output
//   buffer header to the sink component.
// - OmxOutputSink maps the output buffer as a texture for rendering.
// - OmxCodec received dynamic port reconfiguration from the hardware.
// - OmxOutputSink::StopUsingThisBuffer(id) is called to release the buffer.
// - OmxOutputSink unmaps the output buffer as texture and make sure there
//   is not reference to it.
// - OmxOutputSink::UseThisBuffer(id, buffer) is called to assign a new
//   output buffer to the sink component.
// - ...
//
// 1. Buffer is provided by OmxCodec as EGL image.
//
// - OmxOutputSink::AllocateEGLImages(...) is called to request an EGL image
//   from the sink component, an associated buffer header is created.
// - OmxOutputSink::UseThisBuffer(id, buffer) is called to assign an output
//   buffer header to the sink component.
// - OmxOutputSink maps the output buffer as a texture for rendering.
// - OmxCodec received dynamic port reconfiguration from the hardware.
// - OmxOutputSink::StopUsingThisBuffer(id) is called to release the buffer.
// - OmxOutputSink unmaps the output buffer as texture and make sure there
//   is not reference to it.
// - OmxOutputSink::ReleaseEGLImages() is called to tell the sink component
//   to release all allocated EGL images.
// - OmxOutputSink::AllocateEGLImages(...) is called to allocate EGL images.
// - ...
//
// BUFFER READY SIGNALING
//
// Another key part of OmxOutputSink is to process the output buffer given
// by OmxCodec. This is done by signaling the sink component that a
// particular buffer is ready.
//
// This is done through the following sequence:
//
// - Owner of this object calls BufferReady(buffer_id, callback).
// - OmxOutputSink uses the buffer for rendering or other operations
//   asynchronously.
// - Callback provided by BufferReady() is called along with a buffer id to
//   notify the buffer has been consumed.
//
// It is up to implementor to decide which thread this callback is executed.
//
// THREADING
//
// OmxOutputSink::BufferReady(buffer_id) is called from the owner of this
// object which can be made from any thread. All other methods are made on
// the thread where OmxCodec lives.
//
// When the sink component is notified of buffer ready and the buffer is
// used BufferUsedCallback is called. There is no gurantee which thread
// this call is made.
//
// OWNERSHIP
//
// Described in src/media/omx/omx_codec.h.

#ifndef MEDIA_OMX_OMX_OUTPUT_SINK_
#define MEDIA_OMX_OMX_OUTPUT_SINK_

#include <vector>

#include "base/callback.h"
#include "third_party/openmax/il/OMX_Core.h"

// TODO(hclam): This is just to get the build going. Remove this when we
// include the GLES header.
typedef void* EGLImageKHR;

namespace media {

class OmxOutputSink {
 public:
  typedef Callback1<int>::Type BufferUsedCallback;
  virtual ~OmxOutputSink() {}

  // Returns true if this sink component provides EGL images as output
  // buffers.
  virtual bool ProvidesEGLImages() const = 0;

  // Returns a list of EGL images allocated by the sink component. The
  // EGL images will then be given to the hardware decoder for port
  // configuration. The amount of EGL images created is controlled by the
  // sink component. The EGL image allocated is owned by the sink
  // component.
  // Returns true if allocate was successful.
  // TODO(hclam): Improve this API once we know what to do with it.
  virtual bool AllocateEGLImages(int width, int height,
                                 std::vector<EGLImageKHR>* images) = 0;

  // Notify the sink component that OmxCodec has done with the EGL
  // image allocated by AllocateEGLImages(). They can be released by
  // the sink component any time.
  virtual void ReleaseEGLImages(
      const std::vector<EGLImageKHR>& images) = 0;

  // Assign a buffer header to the sink component for output. The sink
  // component will read from the assicated buffer for the decoded frame.
  // There is also an associated buffer id to identify the buffer. This id
  // is used in subsequent steps for identifying the right buffer.
  // Note that the sink component doesn't own the buffer header.
  // Note that this method is used to assign buffer only at the
  // initialization stage and is not used for data delivery.
  virtual void UseThisBuffer(int buffer_id,
                             OMX_BUFFERHEADERTYPE* buffer) = 0;

  // Tell the sink component to stop using the buffer identified by the
  // buffer id.
  // TODO(hclam): Should accept a callback so notify the operation has
  // completed.
  virtual void StopUsingThisBuffer(int id) = 0;

  // Notify the sink component that the buffer identified by buffer id
  // is ready to be consumed. When the buffer is used, |callback| is
  // called. Ths callback can be made on any thread.
  virtual void BufferReady(int buffer_id,
                           BufferUsedCallback* callback) = 0;
};

}

#endif  // MEDIA_OMX_OMX_OUTPUT_SINK_
