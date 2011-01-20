// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multi_process_notification.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
// TODO(dmaclach): Remove defined(OS_MACOSX) once
//                 MultiProcessNotification is implemented on Win/Linux.

namespace {

const char kStartedNotificationName[] = "MultiProcessTestStartedNotification";
const char kQuitNotificationName[] = "MultiProcessTestQuitNotification";

void SpinRunLoop(int milliseconds) {
  MessageLoop *loop = MessageLoop::current();

  // Post a quit task so that this loop eventually ends and we don't hang
  // in the case of a bad test. Usually, the run loop will quit sooner than
  // that because all tests use a MultiProcessNotificationTestQuit which quits
  // the current run loop when it gets a notification.
  loop->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(), milliseconds);
  loop->Run();
}

class SimpleDelegate
    : public multi_process_notification::Listener::Delegate {
 public:
  SimpleDelegate()
      : notification_received_(false), started_received_(false) { }
  virtual ~SimpleDelegate();

  virtual void OnNotificationReceived(
      const std::string& name,
      multi_process_notification::Domain domain) OVERRIDE;
  virtual void OnListenerStarted(
        const std::string& name, multi_process_notification::Domain domain,
        bool success) OVERRIDE;

  bool WasNotificationReceived() { return notification_received_; }
  bool WasStartedReceived() { return started_received_; }

 private:
  bool notification_received_;
  bool started_received_;
  DISALLOW_COPY_AND_ASSIGN(SimpleDelegate);
};

SimpleDelegate::~SimpleDelegate() {
}

void SimpleDelegate::OnNotificationReceived(
    const std::string& name, multi_process_notification::Domain domain) {
  notification_received_ = true;
}

void SimpleDelegate::OnListenerStarted(
    const std::string& name, multi_process_notification::Domain domain,
    bool success) {
  ASSERT_TRUE(success);
  started_received_ = true;
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

class QuitterDelegate : public SimpleDelegate {
 public:
  QuitterDelegate() : SimpleDelegate() { }
  virtual ~QuitterDelegate();

  virtual void OnNotificationReceived(
      const std::string& name,
      multi_process_notification::Domain domain) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitterDelegate);
};

QuitterDelegate::~QuitterDelegate() {
}

void QuitterDelegate::OnNotificationReceived(
    const std::string& name, multi_process_notification::Domain domain) {
  SimpleDelegate::OnNotificationReceived(name, domain);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

int MultiProcessNotificationMain(multi_process_notification::Domain domain) {
  base::Thread thread("MultiProcessNotificationMainIOThread");
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  EXPECT_TRUE(thread.StartWithOptions(options));
  MessageLoop loop;
  QuitterDelegate quitter;
  multi_process_notification::Listener listener(
      kQuitNotificationName, domain, &quitter);
  EXPECT_TRUE(listener.Start(thread.message_loop()));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  EXPECT_TRUE(quitter.WasStartedReceived());
  EXPECT_TRUE(multi_process_notification::Post(kStartedNotificationName,
                                               domain));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  EXPECT_TRUE(quitter.WasNotificationReceived());
  return 0;
}

}  // namespace

class MultiProcessNotificationTest : public base::MultiProcessTest {
 public:
  MultiProcessNotificationTest();

  void PostNotificationTest(multi_process_notification::Domain domain);
  void CrossPostNotificationTest(multi_process_notification::Domain domain);

  virtual void SetUp();
  MessageLoop* IOMessageLoop() { return io_thread_.message_loop(); };

 private:
  MessageLoop loop_;
  base::Thread io_thread_;
};

MultiProcessNotificationTest::MultiProcessNotificationTest()
    : io_thread_("MultiProcessNotificationTestThread") {
}


void MultiProcessNotificationTest::SetUp() {
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread_.StartWithOptions(options));
}

void MultiProcessNotificationTest::PostNotificationTest(
    multi_process_notification::Domain domain) {
  QuitterDelegate process_started;
  multi_process_notification::Listener listener(
      kStartedNotificationName, domain, &process_started);
  ASSERT_TRUE(listener.Start(IOMessageLoop()));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(process_started.WasStartedReceived());
  std::string process_name;
  switch (domain) {
    case multi_process_notification::ProfileDomain:
      process_name = "MultiProcessProfileNotificationMain";
      break;

    case multi_process_notification::UserDomain:
      process_name = "MultiProcessUserNotificationMain";
      break;

    case multi_process_notification::SystemDomain:
      process_name = "MultiProcessSystemNotificationMain";
      break;
  }
  base::ProcessHandle handle = SpawnChild(process_name, false);
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(process_started.WasNotificationReceived());
  ASSERT_TRUE(multi_process_notification::Post(kQuitNotificationName, domain));
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCodeWithTimeout(
      handle, &exit_code, TestTimeouts::action_max_timeout_ms()));
}

void MultiProcessNotificationTest::CrossPostNotificationTest(
    multi_process_notification::Domain domain) {
  // Check to make sure notifications sent to user domain aren't picked up
  // by system domain listeners and vice versa.
  std::string local_notification("QuitLocalNotification");
  std::string final_notification("FinalQuitLocalNotification");
  QuitterDelegate profile_quitter;
  QuitterDelegate user_quitter;
  QuitterDelegate system_quitter;
  QuitterDelegate final_quitter;
  multi_process_notification::Listener profile_listener(
      local_notification, multi_process_notification::ProfileDomain,
      &profile_quitter);
  multi_process_notification::Listener user_listener(
      local_notification, multi_process_notification::UserDomain,
      &user_quitter);
  multi_process_notification::Listener system_listener(
      local_notification, multi_process_notification::SystemDomain,
      &system_quitter);
  multi_process_notification::Listener final_listener(
      final_notification, multi_process_notification::UserDomain,
      &final_quitter);

  MessageLoop* message_loop = IOMessageLoop();
  ASSERT_TRUE(profile_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(profile_quitter.WasStartedReceived());
  ASSERT_TRUE(user_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(user_quitter.WasStartedReceived());
  ASSERT_TRUE(system_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(system_quitter.WasStartedReceived());

  ASSERT_TRUE(multi_process_notification::Post(local_notification, domain));
  SpinRunLoop(TestTimeouts::action_timeout_ms());

  // Now send out a final_notification to queue up a notification
  // after the local_notification and make sure that all listeners have had a
  // chance to process local_notification before we check to see if they
  // were called.
  ASSERT_TRUE(final_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  EXPECT_TRUE(final_quitter.WasStartedReceived());
  ASSERT_TRUE(multi_process_notification::Post(
      final_notification, multi_process_notification::UserDomain));
  SpinRunLoop(TestTimeouts::action_timeout_ms());
  ASSERT_TRUE(final_quitter.WasNotificationReceived());
  switch (domain) {
    case multi_process_notification::ProfileDomain:
      ASSERT_TRUE(profile_quitter.WasNotificationReceived());
      ASSERT_FALSE(user_quitter.WasNotificationReceived());
      ASSERT_FALSE(system_quitter.WasNotificationReceived());
      break;

    case multi_process_notification::UserDomain:
      ASSERT_FALSE(profile_quitter.WasNotificationReceived());
      ASSERT_TRUE(user_quitter.WasNotificationReceived());
      ASSERT_FALSE(system_quitter.WasNotificationReceived());
      break;

    case multi_process_notification::SystemDomain:
      ASSERT_FALSE(profile_quitter.WasNotificationReceived());
      ASSERT_FALSE(user_quitter.WasNotificationReceived());
      ASSERT_TRUE(system_quitter.WasNotificationReceived());
      break;
  }
}

TEST_F(MultiProcessNotificationTest, BasicCreationTest) {
  QuitterDelegate quitter;
  multi_process_notification::Listener local_listener(
      "BasicCreationTest", multi_process_notification::UserDomain, &quitter);
  MessageLoop* message_loop = IOMessageLoop();
  ASSERT_TRUE(local_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasStartedReceived());
  multi_process_notification::Listener system_listener(
      "BasicCreationTest", multi_process_notification::SystemDomain, &quitter);
  ASSERT_TRUE(system_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasStartedReceived());

  multi_process_notification::Listener local_listener2(
      "BasicCreationTest", multi_process_notification::UserDomain, &quitter);
  // Should fail because current message loop should not be an IOMessageLoop.
  ASSERT_FALSE(local_listener2.Start(MessageLoop::current()));
}

TEST_F(MultiProcessNotificationTest, PostInProcessNotification) {
  std::string local_notification("QuitLocalNotification");
  QuitterDelegate quitter;
  multi_process_notification::Listener listener(
      local_notification, multi_process_notification::UserDomain, &quitter);
  MessageLoop* message_loop = IOMessageLoop();
  ASSERT_TRUE(listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasStartedReceived());
  ASSERT_TRUE(multi_process_notification::Post(
      local_notification, multi_process_notification::UserDomain));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasNotificationReceived());
}

TEST_F(MultiProcessNotificationTest, MultiListener) {
  std::string local_notification("LocalNotification");
  std::string quit_local_notification("QuitLocalNotification");

  SimpleDelegate delegate1;
  SimpleDelegate delegate2;
  multi_process_notification::Listener local_listener1(
      local_notification, multi_process_notification::UserDomain,
      &delegate1);
  multi_process_notification::Listener local_listener2(
      local_notification, multi_process_notification::UserDomain,
      &delegate2);

  QuitterDelegate quitter;

  multi_process_notification::Listener quit_listener(quit_local_notification,
      multi_process_notification::UserDomain, &quitter);
  MessageLoop* message_loop = IOMessageLoop();
  ASSERT_TRUE(local_listener1.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(delegate1.WasStartedReceived());
  ASSERT_TRUE(local_listener2.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(delegate2.WasStartedReceived());
  ASSERT_TRUE(quit_listener.Start(message_loop));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasStartedReceived());
  ASSERT_TRUE(multi_process_notification::Post(
      local_notification, multi_process_notification::UserDomain));
  ASSERT_TRUE(multi_process_notification::Post(
      quit_local_notification, multi_process_notification::UserDomain));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(delegate1.WasNotificationReceived());
  ASSERT_TRUE(delegate2.WasNotificationReceived());
  ASSERT_TRUE(quitter.WasNotificationReceived());
}

TEST_F(MultiProcessNotificationTest, PostProfileNotification) {
  PostNotificationTest(multi_process_notification::ProfileDomain);
}

TEST_F(MultiProcessNotificationTest, PostUserNotification) {
  PostNotificationTest(multi_process_notification::UserDomain);
}

TEST_F(MultiProcessNotificationTest, PostSystemNotification) {
  PostNotificationTest(multi_process_notification::SystemDomain);
}

TEST_F(MultiProcessNotificationTest, ProfileCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::ProfileDomain);
}

TEST_F(MultiProcessNotificationTest, UserCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::UserDomain);
}

TEST_F(MultiProcessNotificationTest, SystemCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::SystemDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessProfileNotificationMain) {
  return MultiProcessNotificationMain(
      multi_process_notification::ProfileDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessUserNotificationMain) {
  return MultiProcessNotificationMain(multi_process_notification::UserDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessSystemNotificationMain) {
  return MultiProcessNotificationMain(multi_process_notification::SystemDomain);
}

#endif  // defined(OS_MACOSX)
