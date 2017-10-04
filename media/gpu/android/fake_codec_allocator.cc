// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/fake_codec_allocator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/avda_codec_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

FakeCodecAllocator::FakeCodecAllocator(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : testing::NiceMock<AVDACodecAllocator>(
          base::BindRepeating(&MockMediaCodecBridge::CreateVideoDecoder),
          task_runner) {}

FakeCodecAllocator::~FakeCodecAllocator() = default;

void FakeCodecAllocator::StartThread(AVDACodecAllocatorClient* client) {}

void FakeCodecAllocator::StopThread(AVDACodecAllocatorClient* client) {}

std::unique_ptr<MediaCodecBridge> FakeCodecAllocator::CreateMediaCodecSync(
    scoped_refptr<CodecConfig> config) {
  most_recent_overlay = config->surface_bundle->overlay.get();
  most_recent_surface_texture = config->surface_bundle->surface_texture.get();
  MockCreateMediaCodecSync(most_recent_overlay, most_recent_surface_texture);

  std::unique_ptr<MockMediaCodecBridge> codec;
  if (allow_sync_creation) {
    codec = base::MakeUnique<MockMediaCodecBridge>();
    most_recent_codec = codec.get();
    most_recent_codec_destruction_observer = codec->CreateDestructionObserver();
    most_recent_codec_destruction_observer->DoNotAllowDestruction();
  } else {
    most_recent_codec = nullptr;
    most_recent_codec_destruction_observer = nullptr;
  }

  return std::move(codec);
}

void FakeCodecAllocator::CreateMediaCodecAsync(
    base::WeakPtr<AVDACodecAllocatorClient> client,
    scoped_refptr<CodecConfig> config) {
  // Clear |most_recent_codec| until somebody calls Provide*CodecAsync().
  most_recent_codec = nullptr;
  most_recent_codec_destruction_observer = nullptr;
  most_recent_overlay = config->surface_bundle->overlay.get();
  most_recent_surface_texture = config->surface_bundle->surface_texture.get();
  pending_surface_bundle_ = config->surface_bundle;
  client_ = client;
  codec_creation_pending_ = true;

  MockCreateMediaCodecAsync(most_recent_overlay, most_recent_surface_texture);
}

void FakeCodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<MediaCodecBridge> media_codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  MockReleaseMediaCodec(media_codec.get(), surface_bundle->overlay.get(),
                        surface_bundle->surface_texture.get());
}

MockMediaCodecBridge* FakeCodecAllocator::ProvideMockCodecAsync(
    std::unique_ptr<MockMediaCodecBridge> codec) {
  DCHECK(codec_creation_pending_);
  codec_creation_pending_ = false;

  if (!client_)
    return nullptr;

  auto mock_codec = codec ? std::move(codec)
                          : base::MakeUnique<NiceMock<MockMediaCodecBridge>>();
  auto* raw_codec = mock_codec.get();
  most_recent_codec = raw_codec;
  most_recent_codec_destruction_observer =
      mock_codec->CreateDestructionObserver();
  client_->OnCodecConfigured(std::move(mock_codec),
                             std::move(pending_surface_bundle_));
  return raw_codec;
}

void FakeCodecAllocator::ProvideNullCodecAsync() {
  DCHECK(codec_creation_pending_);
  codec_creation_pending_ = false;
  most_recent_codec = nullptr;
  if (client_)
    client_->OnCodecConfigured(nullptr, std::move(pending_surface_bundle_));
}

}  // namespace media
