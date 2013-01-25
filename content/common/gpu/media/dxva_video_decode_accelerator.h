// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_DXVA_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_DXVA_VIDEO_DECODE_ACCELERATOR_H_

#include <d3d9.h>
#include <dxva2api.h>
#include <list>
#include <map>
#include <mfidl.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/win/scoped_comptr.h"
#include "content/common/content_export.h"
#include "media/video/video_decode_accelerator.h"

interface IMFSample;
interface IDirect3DSurface9;

namespace content {

// Class to provide a DXVA 2.0 based accelerator using the Microsoft Media
// foundation APIs via the VideoDecodeAccelerator interface.
// This class lives on a single thread and DCHECKs that it is never accessed
// from any other.
class CONTENT_EXPORT DXVAVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  enum State {
    kUninitialized,   // un-initialized.
    kNormal,          // normal playing state.
    kResetting,       // upon received Reset(), before ResetDone()
    kStopped,         // upon output EOS received.
    kFlushing,        // upon flush request received.
  };

  // Does not take ownership of |client| which must outlive |*this|.
  explicit DXVAVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Client* client,
      const base::Callback<bool(void)>& make_context_current);
  virtual ~DXVAVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // Initialization work needed before the process is sandboxed.
  // This includes:-
  // 1. Loads the dlls like mf/mfplat/d3d9, etc required for decoding.
  // 2. Setting up the device manager instance which is shared between all
  //    decoder instances.
  // Invokes the completion task, potentially on another thread, when complete.
  static void PreSandboxInitialization(
      const base::Closure& completion_task);

 private:
  typedef void* EGLConfig;
  typedef void* EGLSurface;
  // Creates and initializes an instance of the D3D device and the
  // corresponding device manager. The device manager instance is eventually
  // passed to the IMFTransform interface implemented by the h.264 decoder.
  // Invokes the completion task, potentially on another thread, when complete.
  static void CreateD3DDevManager(
      const base::Closure& completion_task);

  // Creates, initializes and sets the media types for the h.264 decoder.
  bool InitDecoder();

  // Validates whether the h.264 decoder supports hardware video acceleration.
  bool CheckDecoderDxvaSupport();

  // Returns information about the input and output streams. This includes
  // alignment information, decoder support flags, minimum sample size, etc.
  bool GetStreamsInfoAndBufferReqs();

  // Registers the input and output media types on the h.264 decoder. This
  // includes the expected input and output formats.
  bool SetDecoderMediaTypes();

  // Registers the input media type for the h.264 decoder.
  bool SetDecoderInputMediaType();

  // Registers the output media type for the h.264 decoder.
  bool SetDecoderOutputMediaType(const GUID& subtype);

  // Passes a command message to the decoder. This includes commands like
  // start of stream, end of stream, flush, drain the decoder, etc.
  bool SendMFTMessage(MFT_MESSAGE_TYPE msg, int32 param);

  // The bulk of the decoding happens here. This function handles errors,
  // format changes and processes decoded output.
  void DoDecode();

  // Invoked when we have a valid decoded output sample. Retrieves the D3D
  // surface and maintains a copy of it which is passed eventually to the
  // client when we have a picture buffer to copy the surface contents to.
  bool ProcessOutputSample(IMFSample* sample);

  // Processes pending output samples by copying them to available picture
  // slots.
  void ProcessPendingSamples();

  // Helper function to notify the accelerator client about the error.
  void StopOnError(media::VideoDecodeAccelerator::Error error);

  // Transitions the decoder to the uninitialized state. The decoder will stop
  // accepting requests in this state.
  void Invalidate();

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyInputBufferRead(int input_buffer_id);

  // Notifies the client that initialize was completed.
  void NotifyInitializeDone();

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Requests picture buffers from the client.
  void RequestPictureBuffers(int width, int height);

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(const media::Picture& picture);

  // Sends pending input buffer processed acks to the client if we don't have
  // output samples waiting to be processed.
  void NotifyInputBuffersDropped();

  // Decodes pending input buffers.
  void DecodePendingInputBuffers();

  // Helper for handling the Flush operation.
  void FlushInternal();

  // Helper for handling the Decode operation.
  void DecodeInternal(const base::win::ScopedComPtr<IMFSample>& input_sample);

  // To expose client callbacks from VideoDecodeAccelerator.
  media::VideoDecodeAccelerator::Client* client_;

  base::win::ScopedComPtr<IMFTransform> decoder_;

  // These interface pointers are initialized before the process is sandboxed.
  // They are not released when the GPU process exits. This is ok for now
  // because the GPU process does not exit normally on Windows. It is always
  // terminated. The device manager instance is shared among all decoder
  // instances. This is OK because there is internal locking performed by the
  // device manager.
  static IDirect3DDeviceManager9* device_manager_;
  static IDirect3DDevice9Ex* device_;
  static IDirect3DQuery9* query_;
  static IDirect3D9Ex* d3d9_;

  // The EGL config to use for decoded frames.
  EGLConfig egl_config_;

  // Current state of the decoder.
  State state_;

  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;

  // Contains information about a decoded sample.
  struct PendingSampleInfo {
    PendingSampleInfo(int32 buffer_id, IMFSample* sample);
    ~PendingSampleInfo();

    int32 input_buffer_id;
    base::win::ScopedComPtr<IMFSample> output_sample;
  };

  typedef std::list<PendingSampleInfo> PendingOutputSamples;

  // List of decoded output samples.
  PendingOutputSamples pending_output_samples_;

  struct DXVAPictureBuffer;

  // This map maintains the picture buffers passed the client for decoding.
  // The key is the picture buffer id.
  typedef std::map<int32, linked_ptr<DXVAPictureBuffer> > OutputBuffers;
  OutputBuffers output_picture_buffers_;

  // Set to true if we requested picture slots from the client.
  bool pictures_requested_;

  // Ideally the reset token would be a stack variable which is used while
  // creating the device manager. However it seems that the device manager
  // holds onto the token and attempts to access it if the underlying device
  // changes.
  // TODO(ananta): This needs to be verified.
  static uint32 dev_manager_reset_token_;

  // Counter which holds the number of input packets before a successful
  // decode.
  int inputs_before_decode_;

  // Set to true if all necessary initialization needed before the GPU process
  // is sandboxed is done.
  // This includes the following:
  // 1. All required decoder dlls were successfully loaded.
  // 2. The device manager initialization completed.
  static bool pre_sandbox_init_done_;

  // List of input samples waiting to be processed.
  typedef std::list<base::win::ScopedComPtr<IMFSample>> PendingInputs;
  PendingInputs pending_input_buffers_;

  // Callback to set the correct gl context.
  base::Callback<bool(void)> make_context_current_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_DXVA_VIDEO_DECODE_ACCELERATOR_H_
