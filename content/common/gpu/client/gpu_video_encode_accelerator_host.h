// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"

namespace gfx {

class Size;

}  // namespace gfx

namespace media {

class VideoFrame;

}  // namespace media

namespace content {

class GpuChannelHost;

// This class is the renderer-side host for the VideoEncodeAccelerator in the
// GPU process, coordinated over IPC.
class GpuVideoEncodeAcceleratorHost
    : public IPC::Listener,
      public media::VideoEncodeAccelerator,
      public base::SupportsWeakPtr<GpuVideoEncodeAcceleratorHost> {
 public:
  // |client| is assumed to outlive this object.  Since the GpuChannelHost does
  // _not_ own this object, a reference to |gpu_channel_host| is taken.
  GpuVideoEncodeAcceleratorHost(
      media::VideoEncodeAccelerator::Client* client,
      const scoped_refptr<GpuChannelHost>& gpu_channel_host,
      int32 route_id);
  virtual ~GpuVideoEncodeAcceleratorHost();

  // Static query for the supported profiles.  This query proxies to
  // GpuVideoEncodeAccelerator::GetSupportedProfiles().
  static std::vector<SupportedProfile> GetSupportedProfiles();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // media::VideoEncodeAccelerator implementation.
  virtual void Initialize(media::VideoFrame::Format format,
                          const gfx::Size& input_visible_size,
                          media::VideoCodecProfile output_profile,
                          uint32 initial_bitrate) OVERRIDE;
  virtual void Encode(const scoped_refptr<media::VideoFrame>& frame,
                      bool force_keyframe) OVERRIDE;
  virtual void UseOutputBitstreamBuffer(
      const media::BitstreamBuffer& buffer) OVERRIDE;
  virtual void RequestEncodingParametersChange(uint32 bitrate,
                                               uint32 framerate_num) OVERRIDE;
  virtual void Destroy() OVERRIDE;

 private:
  // Notify |client_| when an error has occured.  Used when notifying from
  // within a media::VideoEncodeAccelerator entry point, to avoid re-entrancy.
  void NotifyError(Error error);

  // IPC handlers, proxying media::VideoEncodeAccelerator::Client for the GPU
  // process.
  void OnNotifyInitializeDone();
  void OnRequireBitstreamBuffers(uint32 input_count,
                                 const gfx::Size& input_coded_size,
                                 uint32 output_buffer_size);
  void OnNotifyInputDone(int32 frame_id);
  void OnBitstreamBufferReady(int32 bitstream_buffer_id,
                              uint32 payload_size,
                              bool key_frame);
  void OnNotifyError(Error error);

  void Send(IPC::Message* message);

  // Weak pointer for client callbacks on the
  // media::VideoEncodeAccelerator::Client interface.
  media::VideoEncodeAccelerator::Client* client_;
  // |client_ptr_factory_| is used for callbacks that need to be done through
  // a PostTask() to avoid re-entrancy on the client.
  base::WeakPtrFactory<VideoEncodeAccelerator::Client> client_ptr_factory_;

  // IPC channel and route ID.
  scoped_refptr<GpuChannelHost> channel_;
  const int32 route_id_;

  // media::VideoFrames sent to the encoder.
  // base::IDMap not used here, since that takes pointers, not scoped_refptr.
  typedef base::hash_map<int32, scoped_refptr<media::VideoFrame> > FrameMap;
  FrameMap frame_map_;

  // ID serial number for the next frame to send to the GPU process.
  int32 next_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAcceleratorHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_ENCODE_ACCELERATOR_HOST_H_
