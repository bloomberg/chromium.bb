// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_input_stream_observer.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MojoAudioInputStreamObserverTest : public testing::Test {
 public:
  MojoAudioInputStreamObserverTest() {}
  ~MojoAudioInputStreamObserverTest() override {}

  std::unique_ptr<MojoAudioInputStreamObserver> CreateObserver(
      media::mojom::AudioInputStreamObserverRequest request) {
    return std::make_unique<MojoAudioInputStreamObserver>(
        std::move(request),
        base::BindOnce(
            &MojoAudioInputStreamObserverTest::RecordingStartedCallback,
            base::Unretained(this)),
        base::BindOnce(
            &MojoAudioInputStreamObserverTest::BindingConnectionError,
            base::Unretained(this)),
        nullptr /*user_input_monitor*/);
  }

  MOCK_METHOD0(RecordingStartedCallback, void());
  MOCK_METHOD0(BindingConnectionError, void());

 private:
  base::test::ScopedTaskEnvironment scoped_task_env_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioInputStreamObserverTest);
};

TEST_F(MojoAudioInputStreamObserverTest, DidStartRecording) {
  media::mojom::AudioInputStreamObserverPtr observer_ptr;
  std::unique_ptr<MojoAudioInputStreamObserver> observer =
      CreateObserver(mojo::MakeRequest(&observer_ptr));

  EXPECT_CALL(*this, RecordingStartedCallback());
  observer_ptr->DidStartRecording();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioInputStreamObserverTest, BindingConnectionError) {
  media::mojom::AudioInputStreamObserverPtr observer_ptr;
  std::unique_ptr<MojoAudioInputStreamObserver> observer =
      CreateObserver(mojo::MakeRequest(&observer_ptr));

  EXPECT_CALL(*this, BindingConnectionError());
  observer_ptr.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
