// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_SERVICE_H_
#define CHROME_GPU_GPU_VIDEO_SERVICE_H_

#include <map>

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "chrome/gpu/gpu_video_decoder.h"
#include "ipc/ipc_channel.h"

class GpuChannel;

class GpuVideoService : public IPC::Channel::Listener,
                        public Singleton<GpuVideoService> {
 public:
  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  // TODO(hclam): Remove return value.
  bool CreateVideoDecoder(GpuChannel* channel,
                          MessageRouter* router,
                          int32 decoder_host_id,
                          int32 decoder_id,
                          gpu::gles2::GLES2Decoder* gles2_decoder);
  void DestroyVideoDecoder(MessageRouter* router,
                           int32 decoder_id);

 private:
  struct GpuVideoDecoderInfo {
    scoped_refptr<GpuVideoDecoder> decoder;
    GpuChannel* channel;
  };

  GpuVideoService();
  virtual ~GpuVideoService();

  std::map<int32, GpuVideoDecoderInfo> decoder_map_;

  // Specialize video service on different platform will override.
  virtual bool IntializeGpuVideoService();
  virtual bool UnintializeGpuVideoService();

  friend struct DefaultSingletonTraits<GpuVideoService>;
  DISALLOW_COPY_AND_ASSIGN(GpuVideoService);
};

#endif  // CHROME_GPU_GPU_VIDEO_SERVICE_H_
