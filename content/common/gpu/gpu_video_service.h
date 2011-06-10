// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_VIDEO_SERVICE_H_
#define CONTENT_COMMON_GPU_GPU_VIDEO_SERVICE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "content/common/gpu/gpu_video_decode_accelerator.h"
#include "ipc/ipc_channel.h"

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}  // namespace gles2
}  // namespace gpu

class GpuChannel;
class MessageRouter;

class GpuVideoService : public IPC::Channel::Listener {
 public:
  static GpuVideoService* GetInstance();

  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // TODO(hclam): Remove return value.
  bool CreateVideoDecoder(GpuChannel* channel,
                          MessageRouter* router,
                          int32 decoder_host_id,
                          int32 decoder_id,
                          gpu::gles2::GLES2Decoder* command_decoder,
                          const std::vector<uint32>& configs);
  void DestroyVideoDecoder(MessageRouter* router,
                           int32 decoder_id);

  // Passes given GLES textures to the decoder indicated by id.
  void AssignTexturesToDecoder(int32 decoder_id,
                               const std::vector<int32>& buffer_ids,
                               const std::vector<uint32>& texture_ids,
                               const std::vector<gfx::Size>& sizes);

 private:
  struct VideoDecoderInfo {
    VideoDecoderInfo(scoped_refptr<GpuVideoDecodeAccelerator> g_v_d_a,
                     gpu::gles2::GLES2Decoder* g_d)
        : video_decoder(g_v_d_a),
          command_decoder(g_d) {}
    ~VideoDecoderInfo() {}
    scoped_refptr<GpuVideoDecodeAccelerator> video_decoder;
    gpu::gles2::GLES2Decoder* command_decoder;
  };
  // Map of video and command buffer decoders, indexed by video decoder id.
  typedef std::map<int32, VideoDecoderInfo> DecoderMap;

  GpuVideoService();
  virtual ~GpuVideoService();

  // Specialize video service on different platform will override.
  virtual bool IntializeGpuVideoService();
  virtual bool UnintializeGpuVideoService();

  // Translates a given client texture id to the "real" texture id as recognized
  // in the GPU process.
  bool TranslateTextureForDecoder(
      int32 decoder_id, uint32 client_texture_id, uint32* service_texture_id);

  DecoderMap decoder_map_;

  friend struct DefaultSingletonTraits<GpuVideoService>;
  DISALLOW_COPY_AND_ASSIGN(GpuVideoService);
};

#endif  // CONTENT_COMMON_GPU_GPU_VIDEO_SERVICE_H_
