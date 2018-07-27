// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromecast {
namespace media {

class CastAudioManager;

class CastAudioOutputStream : public ::media::AudioOutputStream {
 public:
  CastAudioOutputStream(
      const ::media::AudioParameters& audio_params,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      // If the |browser_connector| is nullptr, then it will be fetched from the
      // BrowserThread before usage.
      service_manager::Connector* browser_connector,
      CastAudioManager* audio_manager);
  ~CastAudioOutputStream() override;

  // ::media::AudioOutputStream implementation.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* source_callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

  void BindConnectorRequest(
      service_manager::mojom::ConnectorRequest connector_request);

 private:
  class Backend;
  void BindConnectorRequestOnBrowserTaskRunner(
      service_manager::mojom::ConnectorRequest connector_request);

  const ::media::AudioParameters audio_params_;
  scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner_;
  service_manager::Connector* browser_connector_ = nullptr;
  CastAudioManager* const audio_manager_;
  double volume_;

  // Only valid when the stream is open.
  std::unique_ptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioOutputStream);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
