// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_plugin_delegate_impl.h"

#include "app/surface/transport_dib.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"

#if defined(OS_MACOSX)
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#endif

namespace {

// Implements the Image2D using a TransportDIB.
class PlatformImage2DImpl : public pepper::PluginDelegate::PlatformImage2D {
 public:
  // This constructor will take ownership of the dib pointer.
  PlatformImage2DImpl(int width, int height, TransportDIB* dib)
      : width_(width),
        height_(height),
        dib_(dib) {
  }

  virtual skia::PlatformCanvas* Map() {
    return dib_->GetPlatformCanvas(width_, height_);
  }

  virtual intptr_t GetSharedMemoryHandle() const {
    return reinterpret_cast<intptr_t>(dib_.get());
  }

 private:
  int width_;
  int height_;
  scoped_ptr<TransportDIB> dib_;

  DISALLOW_COPY_AND_ASSIGN(PlatformImage2DImpl);
};

class PlatformAudioImpl
    : public pepper::PluginDelegate::PlatformAudio,
      public AudioMessageFilter::Delegate {
 public:
  explicit PlatformAudioImpl(scoped_refptr<AudioMessageFilter> filter)
      : client_(NULL), filter_(filter), stream_id_(0) {
    DCHECK(filter_);
  }

  virtual ~PlatformAudioImpl() {
    // Make sure we have been shut down.
    DCHECK_EQ(0, stream_id_);
    DCHECK(!client_);
  }

  // Initialize this audio context. StreamCreated() will be called when the
  // stream is created.
  bool Initialize(uint32_t sample_rate, uint32_t sample_count,
       pepper::PluginDelegate::PlatformAudio::Client* client);

  virtual bool StartPlayback() {
    return filter_ && filter_->Send(
        new ViewHostMsg_PlayAudioStream(0, stream_id_));
  }

  virtual bool StopPlayback() {
    return filter_ && filter_->Send(
        new ViewHostMsg_PauseAudioStream(0, stream_id_));
  }

  virtual void ShutDown();

 private:
  virtual void OnRequestPacket(uint32 bytes_in_buffer,
                               const base::Time& message_timestamp) {
    LOG(FATAL) << "Should never get OnRequestPacket in PlatformAudioImpl";
  }

  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state) { }

  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length) {
    LOG(FATAL) << "Should never get OnCreated in PlatformAudioImpl";
  }

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);

  virtual void OnVolume(double volume) { }

  // The client to notify when the stream is created.
  pepper::PluginDelegate::PlatformAudio::Client* client_;
  // MessageFilter used to send/receive IPC.
  scoped_refptr<AudioMessageFilter> filter_;
  // Our ID on the MessageFilter.
  int32 stream_id_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioImpl);
};

bool PlatformAudioImpl::Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    pepper::PluginDelegate::PlatformAudio::Client* client) {

  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  ViewHostMsg_Audio_CreateStream_Params params;
  params.format = AudioManager::AUDIO_PCM_LINEAR;
  params.channels = 2;
  params.sample_rate = sample_rate;
  params.bits_per_sample = 16;

  params.packet_size = sample_count * params.channels *
      (params.bits_per_sample >> 3);

  stream_id_ = filter_->AddDelegate(this);
  return filter_->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params,
                                                         true));
}

void PlatformAudioImpl::OnLowLatencyCreated(
    base::SharedMemoryHandle handle, base::SyncSocket::Handle socket_handle,
    uint32 length) {
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, handle.fd);
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);

  client_->StreamCreated(handle, length, socket_handle);
}

void PlatformAudioImpl::ShutDown() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_) {
    return;
  }
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;
  client_ = NULL;
}

}  // namespace

PepperPluginDelegateImpl::PepperPluginDelegateImpl(RenderView* render_view)
    : render_view_(render_view) {
}

void PepperPluginDelegateImpl::ViewInitiatedPaint() {
  // Notify all of our instances that we started painting. This is used for
  // internal bookkeeping only, so we know that the set can not change under
  // us.
  for (std::set<pepper::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->ViewInitiatedPaint();
}

void PepperPluginDelegateImpl::ViewFlushedPaint() {
  // Notify all instances that we painted. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  std::set<pepper::PluginInstance*> plugins = active_instances_;
  for (std::set<pepper::PluginInstance*>::iterator i = plugins.begin();
       i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    //
    // It's possible that a plugin was destroyed, but another one was created
    // with the same address. In this case, we'll call ViewFlushedPaint on that
    // new plugin. But that's OK for this particular case since we're just
    // notifying all of our instances that the view flushed, and the new one is
    // one of our instances.
    //
    // What about the case where a new one is created in a callback at a new
    // address and we don't issue the callback? We're still OK since this
    // callback is used for flush callbacks and we could not have possibly
    // started a new paint (ViewInitiatedPaint) for the new plugin while
    // processing a previous paint for an existing one.
    if (active_instances_.find(*i) != active_instances_.end())
      (*i)->ViewFlushedPaint();
  }
}

void PepperPluginDelegateImpl::InstanceCreated(
    pepper::PluginInstance* instance) {
  active_instances_.insert(instance);
}

void PepperPluginDelegateImpl::InstanceDeleted(
    pepper::PluginInstance* instance) {
  active_instances_.erase(instance);
}

pepper::PluginDelegate::PlatformImage2D*
PepperPluginDelegateImpl::CreateImage2D(int width, int height) {
  uint32 buffer_size = width * height * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code).  Note that the
  // TransportDIB is _not_ cached in the browser; this is because this memory
  // gets flushed by the renderer into another TransportDIB that represents the
  // page, which is then in turn flushed to the screen by the browser process.
  // When |transport_dib_| goes out of scope in the dtor, all of its shared
  // memory gets reclaimed.
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        false,
                                                        &dib_handle);
  if (!RenderThread::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid(dib_handle))
    return NULL;

  TransportDIB* dib = TransportDIB::Map(dib_handle);
#else
  static int next_dib_id = 0;
  TransportDIB* dib = TransportDIB::Create(buffer_size, next_dib_id++);
  if (!dib)
    return NULL;
#endif

  return new PlatformImage2DImpl(width, height, dib);
}

void PepperPluginDelegateImpl::DidChangeNumberOfFindResults(int identifier,
                                                           int total,
                                                           bool final_result) {
  if (total == 0) {
    render_view_->ReportNoFindInPageResults(identifier);
  } else {
    render_view_->reportFindInPageMatchCount(identifier, total, final_result);
  }
}

void PepperPluginDelegateImpl::DidChangeSelectedFindResult(int identifier,
                                                          int index) {
  render_view_->reportFindInPageSelection(
      identifier, index + 1, WebKit::WebRect());
}

pepper::PluginDelegate::PlatformAudio* PepperPluginDelegateImpl::CreateAudio(
    uint32_t sample_rate, uint32_t sample_count,
    pepper::PluginDelegate::PlatformAudio::Client* client) {
  scoped_ptr<PlatformAudioImpl> audio(
      new PlatformAudioImpl(render_view_->audio_message_filter()));
  if (audio->Initialize(sample_rate, sample_count, client)) {
    return audio.release();
  } else {
    return NULL;
  }
}

