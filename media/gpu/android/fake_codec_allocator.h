// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/avda_codec_allocator.h"
#include "media/gpu/android/avda_surface_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// A codec allocator that provides a configurable fake implementation
// and lets you set expecations on the "Mock*" methods.
class FakeCodecAllocator : public testing::NiceMock<AVDACodecAllocator> {
 public:
  FakeCodecAllocator(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FakeCodecAllocator() override;

  void StartThread(AVDACodecAllocatorClient* client) override;
  void StopThread(AVDACodecAllocatorClient* client) override;

  // These are called with some parameters of the codec config by our
  // implementation of their respective functions.  This allows tests to set
  // expectations on them.
  MOCK_METHOD2(MockCreateMediaCodecSync,
               void(AndroidOverlay*, SurfaceTextureGLOwner*));
  MOCK_METHOD2(MockCreateMediaCodecAsync,
               void(AndroidOverlay*, SurfaceTextureGLOwner*));

  // Note that this doesn't exactly match the signature, since unique_ptr
  // doesn't work.  plus, we expand |surface_bundle| a bit to make it more
  // convenient to set expectations.
  MOCK_METHOD3(MockReleaseMediaCodec,
               void(MediaCodecBridge*,
                    AndroidOverlay*,
                    SurfaceTextureGLOwner*));

  std::unique_ptr<MediaCodecBridge> CreateMediaCodecSync(
      scoped_refptr<CodecConfig> config) override;
  void CreateMediaCodecAsync(base::WeakPtr<AVDACodecAllocatorClient> client,
                             scoped_refptr<CodecConfig> config) override;
  void ReleaseMediaCodec(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override;

  // Satisfies the pending codec creation with |codec| if given, or a new
  // MockMediaCodecBridge if not. Returns a raw pointer to the codec, or nullptr
  // if the client WeakPtr was invalidated.
  MockMediaCodecBridge* ProvideMockCodecAsync(
      std::unique_ptr<MockMediaCodecBridge> codec = nullptr);

  // Satisfies the pending codec creation with a null codec.
  void ProvideNullCodecAsync();

  // Most recent codec that we've created via CreateMockCodec, since we have
  // to assign ownership.  It may be freed already.
  MockMediaCodecBridge* most_recent_codec = nullptr;

  // The DestructionObserver for |most_recent_codec|.
  std::unique_ptr<DestructionObserver> most_recent_codec_destruction_observer;

  // The most recent overlay provided during codec allocation.
  AndroidOverlay* most_recent_overlay = nullptr;

  // The most recent surface texture provided during codec allocation.
  SurfaceTextureGLOwner* most_recent_surface_texture = nullptr;

  // Whether CreateMediaCodecSync() is allowed to succeed.
  bool allow_sync_creation = true;

 private:
  // Whether CreateMediaCodecAsync() has been called but a codec hasn't been
  // provided yet.
  bool codec_creation_pending_ = false;
  base::WeakPtr<AVDACodecAllocatorClient> client_;

  // The surface bundle of the pending codec creation.
  scoped_refptr<AVDASurfaceBundle> pending_surface_bundle_;

  DISALLOW_COPY_AND_ASSIGN(FakeCodecAllocator);
};

}  // namespace media
