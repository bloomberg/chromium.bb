// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_DEVICES_H_
#define CHROME_RENDERER_PEPPER_DEVICES_H_
#pragma once

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/simple_thread.h"
#include "chrome/renderer/audio_message_filter.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "ui/gfx/rect.h"

class WebPluginDelegatePepper;
class SkBitmap;

// Lists all contexts currently open for painting. These are ones requested by
// the plugin but not destroyed by it yet. The source pointer is the raw
// pixels. We use this to look up the corresponding transport DIB when the
// plugin tells us to flush or destroy it.
class Graphics2DDeviceContext {
 public:
  explicit Graphics2DDeviceContext(WebPluginDelegatePepper* plugin_delegate);
  ~Graphics2DDeviceContext();

  NPError Initialize(gfx::Rect window_rect,
                     const NPDeviceContext2DConfig* config,
                     NPDeviceContext2D* context);

  NPError Flush(SkBitmap* commited_bitmap, NPDeviceContext2D* context,
                NPDeviceFlushContextCallbackPtr callback, NPP id,
                void* user_data);

  // Notifications that the render view has rendered the page and that it has
  // been flushed to the screen.
  void RenderViewInitiatedPaint();
  void RenderViewFlushedPaint();

  TransportDIB* transport_dib() { return transport_dib_.get(); }
  skia::PlatformCanvas* canvas() { return canvas_.get(); }

 private:
  struct FlushCallbackData;
  typedef std::vector<FlushCallbackData> FlushCallbackVector;

  WebPluginDelegatePepper* plugin_delegate_;

  static int32 next_buffer_id_;
  scoped_ptr<TransportDIB> transport_dib_;

  // The canvas associated with the transport DIB, containing the mapped
  // memory of the image.
  scoped_ptr<skia::PlatformCanvas> canvas_;

  // The plugin may be constantly giving us paint messages. "Unpainted" ones
  // are paint requests which have never been painted. These could have been
  // done while the RenderView was already waiting for an ACK from a previous
  // paint, so won't generate a new one yet.
  //
  // "Painted" ones are those paints that have been painted by RenderView, but
  // for which the ACK from the browser has not yet been received.
  //
  // When we get updates from a plugin with a callback, it is first added to
  // the unpainted callbacks. When the renderer has initiated a paint, we'll
  // move it to the painted callbacks list. When the renderer receives a flush,
  // we'll execute the callback and remove it from the list.
  FlushCallbackVector unpainted_flush_callbacks_;
  FlushCallbackVector painted_flush_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2DDeviceContext);
};


// Each instance of AudioDeviceContext corresponds to one host stream (and one
// audio context). NPDeviceContextAudio contains the id of the context's
// stream in the privatePtr member.
class AudioDeviceContext : public AudioMessageFilter::Delegate,
                           public base::DelegateSimpleThread::Delegate {
 public:
  explicit AudioDeviceContext();
  virtual ~AudioDeviceContext();

  NPError Initialize(AudioMessageFilter* filter,
                     const NPDeviceContextAudioConfig* config,
                     NPDeviceContextAudio* context);

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }
  uint32 shared_memory_size() { return shared_memory_size_; }
  base::SyncSocket* socket() { return socket_.get(); }

 private:

  // AudioMessageFilter::Delegate implementation
  virtual void OnRequestPacket(AudioBuffersState buffers_state);
  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state);
  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length);
  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);
  virtual void OnVolume(double volume);
  virtual void OnDestroy();
  // End of AudioMessageFilter::Delegate implementation

  // DelegateSimpleThread::Delegate implementation
  virtual void Run();
  // End of DelegateSimpleThread::Delegate implementation

  void FireAudioCallback() {
    if (context_ && context_->config.callback) {
      context_->config.callback(context_);
    }
  }

  NPDeviceContextAudio* context_;
  scoped_refptr<AudioMessageFilter> filter_;
  int32 stream_id_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32 shared_memory_size_;
  scoped_ptr<base::SyncSocket> socket_;
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;
};

#endif  // CHROME_RENDERER_PEPPER_DEVICES_H_
