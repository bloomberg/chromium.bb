// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_DEVICES_H_
#define CHROME_RENDERER_PEPPER_DEVICES_H_

#include <stdint.h>

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/gfx/rect.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/renderer/audio_message_filter.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Lists all contexts currently open for painting. These are ones requested by
// the plugin but not destroyed by it yet. The source pointer is the raw
// pixels. We use this to look up the corresponding transport DIB when the
// plugin tells us to flush or destroy it.
class Graphics2DDeviceContext {
 public:
  NPError Initialize(gfx::Rect window_rect,
                     const NPDeviceContext2DConfig* config,
                     NPDeviceContext2D* context);

  NPError Flush(SkBitmap* commited_bitmap, NPDeviceContext2D* context,
                NPDeviceFlushContextCallbackPtr callback, NPP id,
                void* user_data);

 private:
  static int32 next_buffer_id_;
  scoped_ptr<TransportDIB> transport_dib_;

  // The canvas associated with the transport DIB, containing the mapped
  // memory of the image.
  scoped_ptr<skia::PlatformCanvas> canvas_;
};


// Each instance of AudioDeviceContext corresponds to one host stream (and one
// audio context). NPDeviceContextAudio contains the id of the context's
// stream in the privatePtr member.
class AudioDeviceContext : public AudioMessageFilter::Delegate {
 public:
  // TODO(neb): if plugin_delegate parameter is indeed unused, remove it
  explicit AudioDeviceContext() : stream_id_(0) {
  }
  virtual ~AudioDeviceContext();

  NPError Initialize(AudioMessageFilter* filter,
                     const NPDeviceContextAudioConfig* config,
                     NPDeviceContextAudio* context);

  // AudioMessageFilter::Delegate implementation
  virtual void OnRequestPacket(size_t bytes_in_buffer,
                               const base::Time& message_timestamp);
  virtual void OnStateChanged(ViewMsg_AudioStreamState state);
  virtual void OnCreated(base::SharedMemoryHandle handle, size_t length);
  virtual void OnVolume(double volume);
  // End of AudioMessageFilter::Delegate implementation

 private:
  void OnDestroy();

  NPDeviceContextAudio* context_;
  scoped_refptr<AudioMessageFilter> filter_;
  int32 stream_id_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  size_t shared_memory_size_;
};

#endif  // CHROME_RENDERER_PEPPER_DEVICES_H_

