// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/render_view_impl.h"
#include "media/base/media_switches.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebAudioDevice;
using WebKit::WebFrame;
using WebKit::WebVector;
using WebKit::WebView;

namespace content {

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const media::AudioParameters& params,
    int input_channels,
    WebAudioDevice::RenderCallback* callback)
    : params_(params),
      input_channels_(input_channels),
      client_callback_(callback) {
  DCHECK(client_callback_);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!output_device_);
}

void RendererWebAudioDeviceImpl::start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (output_device_)
    return;  // Already started.

  output_device_ = AudioDeviceFactory::NewOutputDevice();

  // TODO(crogers): remove once we properly handle input device selection.
  // https://code.google.com/p/chromium/issues/detail?id=147327
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAudioInput)) {
    // TODO(crogers): support more than hard-coded stereo:
    // https://code.google.com/p/chromium/issues/detail?id=147326
    output_device_->InitializeIO(params_, 2, this);
  } else {
    output_device_->InitializeIO(params_, input_channels_, this);
  }

  // Assumption: This method is being invoked within a V8 call stack.  CHECKs
  // will fail in the call to frameForCurrentContext() otherwise.
  //
  // Therefore, we can perform look-ups to determine which RenderView is
  // starting the audio device.  The reason for all this is because the creator
  // of the WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  WebFrame* const web_frame = WebFrame::frameForCurrentContext();
  WebView* const web_view = web_frame ? web_frame->view() : NULL;
  RenderViewImpl* const render_view =
      web_view ? RenderViewImpl::FromWebView(web_view) : NULL;
  if (render_view) {
    output_device_->SetSourceRenderView(render_view->routing_id());
  }

  output_device_->Start();
  // Note: Default behavior is to auto-play on start.
}

void RendererWebAudioDeviceImpl::stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (output_device_) {
    output_device_->Stop();
    output_device_ = NULL;
  }
}

double RendererWebAudioDeviceImpl::sampleRate() {
  return params_.sample_rate();
}

int RendererWebAudioDeviceImpl::Render(media::AudioBus* dest,
                                       int audio_delay_milliseconds) {
  RenderIO(NULL, dest, audio_delay_milliseconds);
  return dest->frames();
}

void RendererWebAudioDeviceImpl::RenderIO(media::AudioBus* source,
                                          media::AudioBus* dest,
                                          int audio_delay_milliseconds) {
  // Make the client callback for an I/O cycle.
  if (client_callback_) {
    // Wrap the input pointers using WebVector.
    size_t input_channels =
        source ? static_cast<size_t>(source->channels()) : 0;
    WebVector<float*> web_audio_input_data(input_channels);
    for (size_t i = 0; i < input_channels; ++i)
      web_audio_input_data[i] = source->channel(i);

    // Wrap the output pointers using WebVector.
    WebVector<float*> web_audio_data(
        static_cast<size_t>(dest->channels()));
    for (int i = 0; i < dest->channels(); ++i)
      web_audio_data[i] = dest->channel(i);

    client_callback_->render(web_audio_input_data,
                             web_audio_data,
                             dest->frames());
  }
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

}  // namespace content
