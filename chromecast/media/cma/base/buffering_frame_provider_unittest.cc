// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/buffering_frame_provider.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/frame_generator_for_test.h"
#include "chromecast/media/cma/base/mock_frame_consumer.h"
#include "chromecast/media/cma/base/mock_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class BufferingFrameProviderTest : public testing::Test {
 public:
  BufferingFrameProviderTest();
  virtual ~BufferingFrameProviderTest();

  // Setup the test.
  void Configure(
      size_t frame_count,
      const std::vector<bool>& provider_delayed_pattern,
      const std::vector<bool>& consumer_delayed_pattern);

  // Start the test.
  void Start();

 protected:
  scoped_ptr<BufferingFrameProvider> buffering_frame_provider_;
  scoped_ptr<MockFrameConsumer> frame_consumer_;

 private:
  void OnTestTimeout();
  void OnTestCompleted();

  DISALLOW_COPY_AND_ASSIGN(BufferingFrameProviderTest);
};

BufferingFrameProviderTest::BufferingFrameProviderTest() {
}

BufferingFrameProviderTest::~BufferingFrameProviderTest() {
}

void BufferingFrameProviderTest::Configure(
    size_t frame_count,
    const std::vector<bool>& provider_delayed_pattern,
    const std::vector<bool>& consumer_delayed_pattern) {
  DCHECK_GE(frame_count, 1u);

  // Frame generation on the producer and consumer side.
  std::vector<FrameGeneratorForTest::FrameSpec> frame_specs(frame_count);
  for (size_t k = 0; k < frame_specs.size() - 1; k++) {
    frame_specs[k].has_config = (k == 0);
    frame_specs[k].timestamp = base::TimeDelta::FromMilliseconds(40) * k;
    frame_specs[k].size = 512;
    frame_specs[k].has_decrypt_config = ((k % 3) == 0);
  }
  frame_specs[frame_specs.size() - 1].is_eos = true;

  scoped_ptr<FrameGeneratorForTest> frame_generator_provider(
      new FrameGeneratorForTest(frame_specs));
  scoped_ptr<FrameGeneratorForTest> frame_generator_consumer(
      new FrameGeneratorForTest(frame_specs));

  scoped_ptr<MockFrameProvider> frame_provider(new MockFrameProvider());
  frame_provider->Configure(provider_delayed_pattern,
                            frame_generator_provider.Pass());

  size_t max_frame_size = 10 * 1024;
  size_t buffer_size = 10 * max_frame_size;
  buffering_frame_provider_.reset(
      new BufferingFrameProvider(
          scoped_ptr<CodedFrameProvider>(frame_provider.release()),
          buffer_size,
          max_frame_size,
          BufferingFrameProvider::FrameBufferedCB()));

  frame_consumer_.reset(
      new MockFrameConsumer(buffering_frame_provider_.get()));
  frame_consumer_->Configure(
      consumer_delayed_pattern,
      false,
      frame_generator_consumer.Pass());
}

void BufferingFrameProviderTest::Start() {
  frame_consumer_->Start(
      base::Bind(&BufferingFrameProviderTest::OnTestCompleted,
                 base::Unretained(this)));
}

void BufferingFrameProviderTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

void BufferingFrameProviderTest::OnTestCompleted() {
  base::MessageLoop::current()->QuitWhenIdle();
}

TEST_F(BufferingFrameProviderTest, FastProviderSlowConsumer) {
  bool provider_delayed_pattern[] = { false };
  bool consumer_delayed_pattern[] = { true };

  const size_t frame_count = 100u;
  Configure(
      frame_count,
      std::vector<bool>(
          provider_delayed_pattern,
          provider_delayed_pattern + arraysize(provider_delayed_pattern)),
      std::vector<bool>(
          consumer_delayed_pattern,
          consumer_delayed_pattern + arraysize(consumer_delayed_pattern)));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&BufferingFrameProviderTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(BufferingFrameProviderTest, SlowProviderFastConsumer) {
  bool provider_delayed_pattern[] = { true };
  bool consumer_delayed_pattern[] = { false };

  const size_t frame_count = 100u;
  Configure(
      frame_count,
      std::vector<bool>(
          provider_delayed_pattern,
          provider_delayed_pattern + arraysize(provider_delayed_pattern)),
      std::vector<bool>(
          consumer_delayed_pattern,
          consumer_delayed_pattern + arraysize(consumer_delayed_pattern)));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&BufferingFrameProviderTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(BufferingFrameProviderTest, SlowFastProducerConsumer) {
  // Lengths are prime between each other so we can test a lot of combinations.
  bool provider_delayed_pattern[] = {
    true, true, true, true, true,
    false, false, false, false
  };
  bool consumer_delayed_pattern[] = {
    true, true, true, true, true, true, true,
    false, false, false, false, false, false, false
  };

  const size_t frame_count = 100u;
  Configure(
      frame_count,
      std::vector<bool>(
          provider_delayed_pattern,
          provider_delayed_pattern + arraysize(provider_delayed_pattern)),
      std::vector<bool>(
          consumer_delayed_pattern,
          consumer_delayed_pattern + arraysize(consumer_delayed_pattern)));

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&BufferingFrameProviderTest::Start, base::Unretained(this)));
  message_loop->Run();
};

}  // namespace media
}  // namespace chromecast
