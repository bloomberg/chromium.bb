// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_media_client.h"

#include "media/base/android/android_cdm_factory.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "mojo/shell/public/cpp/connect.h"

namespace media {

namespace {

scoped_ptr<ProvisionFetcher> CreateProvisionFetcher(
    mojo::shell::mojom::InterfaceProvider* interface_provider) {
  interfaces::ProvisionFetcherPtr provision_fetcher_ptr;
  mojo::GetInterface(interface_provider, &provision_fetcher_ptr);
  return make_scoped_ptr(
      new MojoProvisionFetcher(std::move(provision_fetcher_ptr)));
}

}  // namespace

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.

scoped_ptr<AudioDecoder> AndroidMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return make_scoped_ptr(new MediaCodecAudioDecoder(task_runner));
}

scoped_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    mojo::shell::mojom::InterfaceProvider* interface_provider) {
  return make_scoped_ptr(new AndroidCdmFactory(
      base::Bind(&CreateProvisionFetcher, interface_provider)));
}

}  // namespace media
