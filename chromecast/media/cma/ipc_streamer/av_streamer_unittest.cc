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
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/frame_generator_for_test.h"
#include "chromecast/media/cma/base/mock_frame_consumer.h"
#include "chromecast/media/cma/base/mock_frame_provider.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc_streamer/av_streamer_proxy.h"
#include "chromecast/media/cma/ipc_streamer/coded_frame_provider_host.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class FifoMemoryChunk : public MediaMemoryChunk {
 public:
  FifoMemoryChunk(void* mem, size_t size)
      : mem_(mem), size_(size) {}
  virtual ~FifoMemoryChunk() {}

  virtual void* data() const OVERRIDE { return mem_; }
  virtual size_t size() const OVERRIDE { return size_; }
  virtual bool valid() const OVERRIDE { return true; }

 private:
  void* mem_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(FifoMemoryChunk);
};

}  // namespace

class AvStreamerTest : public testing::Test {
 public:
  AvStreamerTest();
  virtual ~AvStreamerTest();

  // Setups the test.
  void Configure(
      size_t frame_count,
      const std::vector<bool>& provider_delayed_pattern,
      const std::vector<bool>& consumer_delayed_pattern);

  // Starts the test.
  void Start();

 protected:
  scoped_ptr<uint64[]> fifo_mem_;

  scoped_ptr<AvStreamerProxy> av_buffer_proxy_;
  scoped_ptr<CodedFrameProviderHost> coded_frame_provider_host_;
  scoped_ptr<MockFrameConsumer> frame_consumer_;

 private:
  void OnTestTimeout();
  void OnTestCompleted();

  void OnFifoRead();
  void OnFifoWrite();

  DISALLOW_COPY_AND_ASSIGN(AvStreamerTest);
};

AvStreamerTest::AvStreamerTest() {
}

AvStreamerTest::~AvStreamerTest() {
}

void AvStreamerTest::Configure(
    size_t frame_count,
    const std::vector<bool>& provider_delayed_pattern,
    const std::vector<bool>& consumer_delayed_pattern) {
  // Frame generation on the producer and consumer side.
  std::vector<FrameGeneratorForTest::FrameSpec> frame_specs;
  frame_specs.resize(frame_count);
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

  size_t fifo_size_div_8 = 512;
  fifo_mem_.reset(new uint64[fifo_size_div_8]);
  scoped_ptr<MediaMessageFifo> producer_fifo(
      new MediaMessageFifo(
          scoped_ptr<MediaMemoryChunk>(
              new FifoMemoryChunk(&fifo_mem_[0], fifo_size_div_8 * 8)),
          true));
  scoped_ptr<MediaMessageFifo> consumer_fifo(
      new MediaMessageFifo(
          scoped_ptr<MediaMemoryChunk>(
              new FifoMemoryChunk(&fifo_mem_[0], fifo_size_div_8 * 8)),
          false));
  producer_fifo->ObserveWriteActivity(
      base::Bind(&AvStreamerTest::OnFifoWrite, base::Unretained(this)));
  consumer_fifo->ObserveReadActivity(
      base::Bind(&AvStreamerTest::OnFifoRead, base::Unretained(this)));

  av_buffer_proxy_.reset(
      new AvStreamerProxy());
  av_buffer_proxy_->SetCodedFrameProvider(
      scoped_ptr<CodedFrameProvider>(frame_provider.release()));
  av_buffer_proxy_->SetMediaMessageFifo(producer_fifo.Pass());

  coded_frame_provider_host_.reset(
      new CodedFrameProviderHost(consumer_fifo.Pass()));

  frame_consumer_.reset(
      new MockFrameConsumer(coded_frame_provider_host_.get()));
  frame_consumer_->Configure(
      consumer_delayed_pattern,
      false,
      frame_generator_consumer.Pass());
}

void AvStreamerTest::Start() {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerProxy::Start,
                 base::Unretained(av_buffer_proxy_.get())));

  frame_consumer_->Start(
      base::Bind(&AvStreamerTest::OnTestCompleted,
                 base::Unretained(this)));
}

void AvStreamerTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

void AvStreamerTest::OnTestCompleted() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void AvStreamerTest::OnFifoWrite() {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&CodedFrameProviderHost::OnFifoWriteEvent,
                 base::Unretained(coded_frame_provider_host_.get())));
}

void AvStreamerTest::OnFifoRead() {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerProxy::OnFifoReadEvent,
                 base::Unretained(av_buffer_proxy_.get())));
}

TEST_F(AvStreamerTest, FastProviderSlowConsumer) {
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
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(AvStreamerTest, SlowProviderFastConsumer) {
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
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(AvStreamerTest, SlowFastProducerConsumer) {
  // Pattern lengths are prime between each other
  // so that a lot of combinations can be tested.
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
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

}  // namespace media
}  // namespace chromecast
