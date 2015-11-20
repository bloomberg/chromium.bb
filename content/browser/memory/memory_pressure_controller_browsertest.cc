// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/browser/memory/memory_pressure_controller.h"
#include "content/common/memory_messages.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

MATCHER_P(IsSetSuppressedMessage, suppressed, "") {
  // Ensure that the message is deleted upon return.
  scoped_ptr<IPC::Message> message(arg);
  if (message == nullptr)
    return false;
  MemoryMsg_SetPressureNotificationsSuppressed::Param param;
  if (!MemoryMsg_SetPressureNotificationsSuppressed::Read(message.get(),
                                                          &param))
    return false;
  return suppressed == base::get<0>(param);
}

MATCHER_P(IsSimulateMessage, level, "") {
  // Ensure that the message is deleted upon return.
  scoped_ptr<IPC::Message> message(arg);
  if (message == nullptr)
    return false;
  MemoryMsg_SimulatePressureNotification::Param param;
  if (!MemoryMsg_SimulatePressureNotification::Read(message.get(), &param))
    return false;
  return level == base::get<0>(param);
}

class MemoryMessageFilterForTesting : public MemoryMessageFilter {
 public:
  MOCK_METHOD1(Send, bool(IPC::Message* message));

  void Add() {
    // The filter must be added on the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MemoryMessageFilterForTesting::Add,
                                         base::Unretained(this)));
      RunAllPendingInMessageLoop(BrowserThread::IO);
      return;
    }
    OnChannelConnected(0);
    OnFilterAdded(nullptr);
  }

  void Remove() {
    // The filter must be removed on the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MemoryMessageFilterForTesting::Remove,
                                         base::Unretained(this)));
      RunAllPendingInMessageLoop(BrowserThread::IO);
      return;
    }
    OnChannelClosing();
    OnFilterRemoved();
  }

 protected:
  ~MemoryMessageFilterForTesting() override {}
};

class MemoryPressureControllerBrowserTest : public ContentBrowserTest {
 public:
  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

 protected:
  void SetPressureNotificationsSuppressedInAllProcessesAndWait(
      bool suppressed) {
    MemoryPressureController::GetInstance()
        ->SetPressureNotificationsSuppressedInAllProcesses(suppressed);
    RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void SimulatePressureNotificationInAllProcessesAndWait(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    MemoryPressureController::GetInstance()
        ->SimulatePressureNotificationInAllProcesses(level);
    RunAllPendingInMessageLoop(BrowserThread::IO);
  }
};

IN_PROC_BROWSER_TEST_F(MemoryPressureControllerBrowserTest,
                       SetPressureNotificationsSuppressedInAllProcesses) {
  scoped_refptr<MemoryMessageFilterForTesting> filter1(
      new MemoryMessageFilterForTesting);
  scoped_refptr<MemoryMessageFilterForTesting> filter2(
      new MemoryMessageFilterForTesting);

  NavigateToURL(shell(), GetTestUrl("", "title.html"));

  // Add the first filter. No messages should be sent because notifications are
  // not suppressed.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter1->Add();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // Enable suppressing memory pressure notifications in all processes. The
  // first filter should send a message.
  EXPECT_CALL(*filter1, Send(IsSetSuppressedMessage(true))).Times(1);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // Add the second filter. It should send a message because notifications are
  // suppressed.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(true))).Times(1);
  filter2->Add();

  // Disable suppressing memory pressure notifications in all processes. Both
  // filters should send a message.
  EXPECT_CALL(*filter1, Send(IsSetSuppressedMessage(false))).Times(1);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(false))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // Remove the first filter. No messages should be sent.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter1->Remove();

  // Enable suppressing memory pressure notifications in all processes. The
  // second filter should send a message.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(true))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // Remove the second filter and disable suppressing memory pressure
  // notifications in all processes. No messages should be sent.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter2->Remove();
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
}

IN_PROC_BROWSER_TEST_F(MemoryPressureControllerBrowserTest,
                       SimulatePressureNotificationInAllProcesses) {
  const auto MEMORY_PRESSURE_LEVEL_MODERATE =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  const auto MEMORY_PRESSURE_LEVEL_CRITICAL =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

  scoped_refptr<MemoryMessageFilterForTesting> filter(
      new MemoryMessageFilterForTesting);
  scoped_ptr<base::MemoryPressureListener> listener(
      new base::MemoryPressureListener(
          base::Bind(&MemoryPressureControllerBrowserTest::OnMemoryPressure,
                     base::Unretained(this))));

  NavigateToURL(shell(), GetTestUrl("", "title.html"));

  filter->Add();

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_CRITICAL)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.

  // Enable suppressing memory pressure notifications in all processes. This
  // should have no impact on simulating memory pressure notifications.
  EXPECT_CALL(*filter, Send(IsSetSuppressedMessage(true))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.

  // Disable suppressing memory pressure notifications in all processes. This
  // should have no impact on simulating memory pressure notifications.
  EXPECT_CALL(*filter, Send(IsSetSuppressedMessage(false))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.

  filter->Remove();
}

}  // namespace content
