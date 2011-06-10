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
                          const std::vector<uint32>& configs);
  void DestroyVideoDecoder(MessageRouter* router,
                           int32 decoder_id);

 private:
  GpuVideoService();
  virtual ~GpuVideoService();

  std::map<int32, scoped_refptr<GpuVideoDecodeAccelerator> > decoder_map_;

  // Specialize video service on different platform will override.
  virtual bool IntializeGpuVideoService();
  virtual bool UnintializeGpuVideoService();

  friend struct DefaultSingletonTraits<GpuVideoService>;
  DISALLOW_COPY_AND_ASSIGN(GpuVideoService);
};

#endif  // CONTENT_COMMON_GPU_GPU_VIDEO_SERVICE_H_
