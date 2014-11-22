// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_manager.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/system/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class ChannelManagerTest : public testing::Test {
 public:
  ChannelManagerTest() : message_loop_(base::MessageLoop::TYPE_IO) {}
  ~ChannelManagerTest() override {}

 protected:
  embedder::SimplePlatformSupport* platform_support() {
    return &platform_support_;
  }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  embedder::SimplePlatformSupport platform_support_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelManagerTest);
};

TEST_F(ChannelManagerTest, Basic) {
  ChannelManager cm;

  // Hang on to a ref to the |Channel|, so that we can check that the
  // |ChannelManager| takes/releases refs to it.
  scoped_refptr<Channel> ch(new Channel(platform_support()));
  ASSERT_TRUE(ch->HasOneRef());

  embedder::PlatformChannelPair channel_pair;
  ch->Init(RawChannel::Create(channel_pair.PassServerHandle()));

  ChannelId id = cm.AddChannel(ch, base::MessageLoopProxy::current());
  EXPECT_NE(id, 0u);
  // |ChannelManager| should take a ref.
  EXPECT_FALSE(ch->HasOneRef());

  cm.WillShutdownChannel(id);
  // |ChannelManager| should still have a ref.
  EXPECT_FALSE(ch->HasOneRef());

  cm.ShutdownChannel(id);
  // On the "I/O" thread, so shutdown should happen synchronously.
  // |ChannelManager| should have given up its ref.
  EXPECT_TRUE(ch->HasOneRef());
}

TEST_F(ChannelManagerTest, TwoChannels) {
  ChannelManager cm;

  // Hang on to a ref to each |Channel|, so that we can check that the
  // |ChannelManager| takes/releases refs to them.
  scoped_refptr<Channel> ch1(new Channel(platform_support()));
  ASSERT_TRUE(ch1->HasOneRef());
  scoped_refptr<Channel> ch2(new Channel(platform_support()));
  ASSERT_TRUE(ch2->HasOneRef());

  embedder::PlatformChannelPair channel_pair;
  ch1->Init(RawChannel::Create(channel_pair.PassServerHandle()));
  ch2->Init(RawChannel::Create(channel_pair.PassClientHandle()));

  ChannelId id1 = cm.AddChannel(ch1, base::MessageLoopProxy::current());
  EXPECT_NE(id1, 0u);
  EXPECT_FALSE(ch1->HasOneRef());

  ChannelId id2 = cm.AddChannel(ch2, base::MessageLoopProxy::current());
  EXPECT_NE(id2, 0u);
  EXPECT_NE(id2, id1);
  EXPECT_FALSE(ch2->HasOneRef());

  // Calling |WillShutdownChannel()| multiple times (on |id1|) is okay.
  cm.WillShutdownChannel(id1);
  cm.WillShutdownChannel(id1);
  EXPECT_FALSE(ch1->HasOneRef());
  // Not calling |WillShutdownChannel()| (on |id2|) is okay too.

  cm.ShutdownChannel(id1);
  EXPECT_TRUE(ch1->HasOneRef());
  cm.ShutdownChannel(id2);
  EXPECT_TRUE(ch2->HasOneRef());
}

class OtherThread : public base::SimpleThread {
 public:
  // Note: We rely on the main thread keeping *exactly one* reference to
  // |channel|.
  OtherThread(scoped_refptr<base::TaskRunner> task_runner,
              ChannelManager* channel_manager,
              Channel* channel,
              base::Closure quit_closure)
      : base::SimpleThread("other_thread"),
        task_runner_(task_runner),
        channel_manager_(channel_manager),
        channel_(channel),
        quit_closure_(quit_closure) {}
  ~OtherThread() override {}

 private:
  void Run() override {
    // See comment above constructor.
    ASSERT_TRUE(channel_->HasOneRef());

    ChannelId id = channel_manager_->AddChannel(make_scoped_refptr(channel_),
                                                task_runner_);
    EXPECT_NE(id, 0u);
    // |ChannelManager| should take a ref.
    EXPECT_FALSE(channel_->HasOneRef());

    channel_manager_->WillShutdownChannel(id);
    // |ChannelManager| should still have a ref.
    EXPECT_FALSE(channel_->HasOneRef());

    channel_manager_->ShutdownChannel(id);
    // This doesn't happen synchronously, so we "wait" until it does.
    // TODO(vtl): Possibly |Channel| should provide some notification of being
    // shut down.
    base::TimeTicks start_time(base::TimeTicks::Now());
    for (;;) {
      if (channel_->HasOneRef())
        break;

      // Check, instead of assert, since if things go wrong, dying is more
      // reliable than tearing down.
      CHECK_LT(base::TimeTicks::Now() - start_time,
               TestTimeouts::action_timeout());
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
    }

    CHECK(task_runner_->PostTask(FROM_HERE, quit_closure_));
  }

  scoped_refptr<base::TaskRunner> task_runner_;
  ChannelManager* channel_manager_;
  Channel* channel_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(OtherThread);
};

TEST_F(ChannelManagerTest, CallsFromOtherThread) {
  ChannelManager cm;

  // Hang on to a ref to the |Channel|, so that we can check that the
  // |ChannelManager| takes/releases refs to it.
  scoped_refptr<Channel> ch(new Channel(platform_support()));
  ASSERT_TRUE(ch->HasOneRef());

  embedder::PlatformChannelPair channel_pair;
  ch->Init(RawChannel::Create(channel_pair.PassServerHandle()));

  base::RunLoop run_loop;
  OtherThread thread(base::MessageLoopProxy::current(), &cm, ch.get(),
                     run_loop.QuitClosure());
  thread.Start();
  run_loop.Run();
  thread.Join();
}

}  // namespace
}  // namespace system
}  // namespace mojo
