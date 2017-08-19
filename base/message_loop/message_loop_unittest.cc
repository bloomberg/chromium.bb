// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/android/java_handler_thread_for_testing.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#include "base/process/memory.h"
#include "base/strings/string16.h"
#include "base/win/current_module.h"
#include "base/win/scoped_handle.h"
#endif

namespace base {

// TODO(darin): Platform-specific MessageLoop tests should be grouped together
// to avoid chopping this file up with so many #ifdefs.

namespace {

class Foo : public RefCounted<Foo> {
 public:
  Foo() : test_count_(0) {
  }

  void Test0() { ++test_count_; }

  void Test1ConstRef(const std::string& a) {
    ++test_count_;
    result_.append(a);
  }

  void Test1Ptr(std::string* a) {
    ++test_count_;
    result_.append(*a);
  }

  void Test1Int(int a) { test_count_ += a; }

  void Test2Ptr(std::string* a, std::string* b) {
    ++test_count_;
    result_.append(*a);
    result_.append(*b);
  }

  void Test2Mixed(const std::string& a, std::string* b) {
    ++test_count_;
    result_.append(a);
    result_.append(*b);
  }

  int test_count() const { return test_count_; }
  const std::string& result() const { return result_; }

 private:
  friend class RefCounted<Foo>;

  ~Foo() {}

  int test_count_;
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(Foo);
};

// This function runs slowly to simulate a large amount of work being done.
static void SlowFunc(TimeDelta pause, int* quit_counter) {
  PlatformThread::Sleep(pause);
  if (--(*quit_counter) == 0)
    RunLoop::QuitCurrentWhenIdleDeprecated();
}

// This function records the time when Run was called in a Time object, which is
// useful for building a variety of MessageLoop tests.
static void RecordRunTimeFunc(TimeTicks* run_time, int* quit_counter) {
  *run_time = TimeTicks::Now();

  // Cause our Run function to take some time to execute.  As a result we can
  // count on subsequent RecordRunTimeFunc()s running at a future time,
  // without worry about the resolution of our system clock being an issue.
  SlowFunc(TimeDelta::FromMilliseconds(10), quit_counter);
}

enum TaskType {
  MESSAGEBOX,
  ENDDIALOG,
  RECURSIVE,
  TIMEDMESSAGELOOP,
  QUITMESSAGELOOP,
  ORDERED,
  PUMPS,
  SLEEP,
  RUNS,
};

// Saves the order in which the tasks executed.
struct TaskItem {
  TaskItem(TaskType t, int c, bool s)
      : type(t),
        cookie(c),
        start(s) {
  }

  TaskType type;
  int cookie;
  bool start;

  bool operator == (const TaskItem& other) const {
    return type == other.type && cookie == other.cookie && start == other.start;
  }
};

std::ostream& operator <<(std::ostream& os, TaskType type) {
  switch (type) {
  case MESSAGEBOX:        os << "MESSAGEBOX"; break;
  case ENDDIALOG:         os << "ENDDIALOG"; break;
  case RECURSIVE:         os << "RECURSIVE"; break;
  case TIMEDMESSAGELOOP:  os << "TIMEDMESSAGELOOP"; break;
  case QUITMESSAGELOOP:   os << "QUITMESSAGELOOP"; break;
  case ORDERED:          os << "ORDERED"; break;
  case PUMPS:             os << "PUMPS"; break;
  case SLEEP:             os << "SLEEP"; break;
  default:
    NOTREACHED();
    os << "Unknown TaskType";
    break;
  }
  return os;
}

std::ostream& operator <<(std::ostream& os, const TaskItem& item) {
  if (item.start)
    return os << item.type << " " << item.cookie << " starts";
  else
    return os << item.type << " " << item.cookie << " ends";
}

class TaskList {
 public:
  void RecordStart(TaskType type, int cookie) {
    TaskItem item(type, cookie, true);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  void RecordEnd(TaskType type, int cookie) {
    TaskItem item(type, cookie, false);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  size_t Size() {
    return task_list_.size();
  }

  TaskItem Get(int n)  {
    return task_list_[n];
  }

 private:
  std::vector<TaskItem> task_list_;
};

class DummyTaskObserver : public MessageLoop::TaskObserver {
 public:
  explicit DummyTaskObserver(int num_tasks)
      : num_tasks_started_(0), num_tasks_processed_(0), num_tasks_(num_tasks) {}

  DummyTaskObserver(int num_tasks, int num_tasks_started)
      : num_tasks_started_(num_tasks_started),
        num_tasks_processed_(0),
        num_tasks_(num_tasks) {}

  ~DummyTaskObserver() override {}

  void WillProcessTask(const PendingTask& pending_task) override {
    num_tasks_started_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_ + 1);
  }

  void DidProcessTask(const PendingTask& pending_task) override {
    num_tasks_processed_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_);
  }

  int num_tasks_started() const { return num_tasks_started_; }
  int num_tasks_processed() const { return num_tasks_processed_; }

 private:
  int num_tasks_started_;
  int num_tasks_processed_;
  const int num_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskObserver);
};

void RecursiveFunc(TaskList* order, int cookie, int depth,
                   bool is_reentrant) {
  order->RecordStart(RECURSIVE, cookie);
  if (depth > 0) {
    if (is_reentrant)
      MessageLoop::current()->SetNestableTasksAllowed(true);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        BindOnce(&RecursiveFunc, order, cookie, depth - 1, is_reentrant));
  }
  order->RecordEnd(RECURSIVE, cookie);
}

void QuitFunc(TaskList* order, int cookie) {
  order->RecordStart(QUITMESSAGELOOP, cookie);
  RunLoop::QuitCurrentWhenIdleDeprecated();
  order->RecordEnd(QUITMESSAGELOOP, cookie);
}

void PostNTasks(int posts_remaining) {
  if (posts_remaining > 1) {
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&PostNTasks, posts_remaining - 1));
  }
}

#if defined(OS_ANDROID)
void AbortMessagePump() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jclass exception = env->FindClass(
      "org/chromium/base/TestSystemMessageHandler$TestException");

  env->ThrowNew(exception,
                "This is a test exception that should be caught in "
                "TestSystemMessageHandler.handleMessage");
  static_cast<base::MessageLoopForUI*>(base::MessageLoop::current())->Abort();
}

void RunTest_AbortDontRunMoreTasks(bool delayed, bool init_java_first) {
  WaitableEvent test_done_event(WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<android::JavaHandlerThread> java_thread;
  if (init_java_first) {
    java_thread =
        android::JavaHandlerThreadForTesting::CreateJavaFirst(&test_done_event);
  } else {
    java_thread = android::JavaHandlerThreadForTesting::Create(
        "JavaHandlerThreadForTesting from AbortDontRunMoreTasks",
        &test_done_event);
  }
  java_thread->Start();

  if (delayed) {
    java_thread->message_loop()->task_runner()->PostDelayedTask(
        FROM_HERE, BindOnce(&AbortMessagePump),
        TimeDelta::FromMilliseconds(10));
  } else {
    java_thread->message_loop()->task_runner()->PostTask(
        FROM_HERE, BindOnce(&AbortMessagePump));
  }

  // Wait to ensure we catch the correct exception (and don't crash)
  test_done_event.Wait();

  java_thread->Stop();
  java_thread.reset();
}

TEST(MessageLoopTest, JavaExceptionAbort) {
  constexpr bool delayed = false;
  constexpr bool init_java_first = false;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}
TEST(MessageLoopTest, DelayedJavaExceptionAbort) {
  constexpr bool delayed = true;
  constexpr bool init_java_first = false;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}
TEST(MessageLoopTest, JavaExceptionAbortInitJavaFirst) {
  constexpr bool delayed = false;
  constexpr bool init_java_first = true;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}

TEST(MessageLoopTest, RunTasksWhileShuttingDownJavaThread) {
  const int kNumPosts = 6;
  DummyTaskObserver observer(kNumPosts, 1);

  auto java_thread = MakeUnique<android::JavaHandlerThread>("test");
  java_thread->Start();

  java_thread->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](android::JavaHandlerThread* java_thread,
             DummyTaskObserver* observer, int num_posts) {
            java_thread->message_loop()->AddTaskObserver(observer);
            ThreadTaskRunnerHandle::Get()->PostDelayedTask(
                FROM_HERE, BindOnce([]() { ADD_FAILURE(); }),
                TimeDelta::FromDays(1));
            java_thread->StopMessageLoopForTesting();
            PostNTasks(num_posts);
          },
          Unretained(java_thread.get()), Unretained(&observer), kNumPosts));

  java_thread->JoinForTesting();
  java_thread.reset();

  EXPECT_EQ(kNumPosts, observer.num_tasks_started());
  EXPECT_EQ(kNumPosts, observer.num_tasks_processed());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)

void SubPumpFunc() {
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  RunLoop::QuitCurrentWhenIdleDeprecated();
}

void RunTest_PostDelayedTask_SharedTimer_SubPump() {
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  TimeTicks run_time;

  message_loop.task_runner()->PostTask(FROM_HERE, BindOnce(&SubPumpFunc));

  // This very delayed task should never run.
  message_loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromSeconds(1000));

  // This slightly delayed task should run from within SubPumpFunc.
  message_loop.task_runner()->PostDelayedTask(FROM_HERE,
                                              BindOnce(&PostQuitMessage, 0),
                                              TimeDelta::FromMilliseconds(10));

  Time start_time = Time::Now();

  RunLoop().Run();
  EXPECT_EQ(1, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = Time::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time.is_null());
}

const wchar_t kMessageBoxTitle[] = L"MessageLoop Unit Test";

// MessageLoop implicitly start a "modal message loop". Modal dialog boxes,
// common controls (like OpenFile) and StartDoc printing function can cause
// implicit message loops.
void MessageBoxFunc(TaskList* order, int cookie, bool is_reentrant) {
  order->RecordStart(MESSAGEBOX, cookie);
  if (is_reentrant)
    MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageBox(NULL, L"Please wait...", kMessageBoxTitle, MB_OK);
  order->RecordEnd(MESSAGEBOX, cookie);
}

// Will end the MessageBox.
void EndDialogFunc(TaskList* order, int cookie) {
  order->RecordStart(ENDDIALOG, cookie);
  HWND window = GetActiveWindow();
  if (window != NULL) {
    EXPECT_NE(EndDialog(window, IDCONTINUE), 0);
    // Cheap way to signal that the window wasn't found if RunEnd() isn't
    // called.
    order->RecordEnd(ENDDIALOG, cookie);
  }
}

void RecursiveFuncWin(scoped_refptr<SingleThreadTaskRunner> task_runner,
                      HANDLE event,
                      bool expect_window,
                      TaskList* order,
                      bool is_reentrant) {
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&RecursiveFunc, order, 1, 2, is_reentrant));
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&MessageBoxFunc, order, 2, is_reentrant));
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&RecursiveFunc, order, 3, 2, is_reentrant));
  // The trick here is that for recursive task processing, this task will be
  // ran _inside_ the MessageBox message loop, dismissing the MessageBox
  // without a chance.
  // For non-recursive task processing, this will be executed _after_ the
  // MessageBox will have been dismissed by the code below, where
  // expect_window_ is true.
  task_runner->PostTask(FROM_HERE, BindOnce(&EndDialogFunc, order, 4));
  task_runner->PostTask(FROM_HERE, BindOnce(&QuitFunc, order, 5));

  // Enforce that every tasks are sent before starting to run the main thread
  // message loop.
  ASSERT_TRUE(SetEvent(event));

  // Poll for the MessageBox. Don't do this at home! At the speed we do it,
  // you will never realize one MessageBox was shown.
  for (; expect_window;) {
    HWND window = FindWindow(L"#32770", kMessageBoxTitle);
    if (window) {
      // Dismiss it.
      for (;;) {
        HWND button = FindWindowEx(window, NULL, L"Button", NULL);
        if (button != NULL) {
          EXPECT_EQ(0, SendMessage(button, WM_LBUTTONDOWN, 0, 0));
          EXPECT_EQ(0, SendMessage(button, WM_LBUTTONUP, 0, 0));
          break;
        }
      }
      break;
    }
  }
}

// TODO(darin): These tests need to be ported since they test critical
// message loop functionality.

// A side effect of this test is the generation a beep. Sorry.
void RunTest_RecursiveDenial2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveDenial2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFuncWin, ThreadTaskRunnerHandle::Get(),
                          event.Get(), true, &order, false));
  // Let the other thread execute.
  WaitForSingleObject(event.Get(), INFINITE);
  RunLoop().Run();

  ASSERT_EQ(17u, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(MESSAGEBOX, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(RECURSIVE, 3, false));
  // When EndDialogFunc is processed, the window is already dismissed, hence no
  // "end" entry.
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(7), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, false));
}

// A side effect of this test is the generation a beep. Sorry.  This test also
// needs to process windows messages on the current thread.
void RunTest_RecursiveSupport2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveSupport2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFuncWin, ThreadTaskRunnerHandle::Get(),
                          event.Get(), false, &order, true));
  // Let the other thread execute.
  WaitForSingleObject(event.Get(), INFINITE);
  RunLoop().Run();

  ASSERT_EQ(18u, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(MESSAGEBOX, 2, true));
  // Note that this executes in the MessageBox modal loop.
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(5), TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order.Get(6), TaskItem(ENDDIALOG, 4, false));
  EXPECT_EQ(order.Get(7), TaskItem(MESSAGEBOX, 2, false));
  /* The order can subtly change here. The reason is that when RecursiveFunc(1)
     is called in the main thread, if it is faster than getting to the
     PostTask(FROM_HERE, BindOnce(&QuitFunc) execution, the order of task
     execution can change. We don't care anyway that the order isn't correct.
  EXPECT_EQ(order.Get(8), TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order.Get(9), TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  */
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(16), TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order.Get(17), TaskItem(RECURSIVE, 3, false));
}

#endif  // defined(OS_WIN)

void PostNTasksThenQuit(int posts_remaining) {
  if (posts_remaining > 1) {
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&PostNTasksThenQuit, posts_remaining - 1));
  } else {
    RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

#if defined(OS_WIN)

class TestIOHandler : public MessageLoopForIO::IOHandler {
 public:
  TestIOHandler(const wchar_t* name, HANDLE signal, bool wait);

  void OnIOCompleted(MessageLoopForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

  void Init();
  void WaitForIO();
  OVERLAPPED* context() { return &context_.overlapped; }
  DWORD size() { return sizeof(buffer_); }

 private:
  char buffer_[48];
  MessageLoopForIO::IOContext context_;
  HANDLE signal_;
  win::ScopedHandle file_;
  bool wait_;
};

TestIOHandler::TestIOHandler(const wchar_t* name, HANDLE signal, bool wait)
    : signal_(signal), wait_(wait) {
  memset(buffer_, 0, sizeof(buffer_));

  file_.Set(CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, NULL));
  EXPECT_TRUE(file_.IsValid());
}

void TestIOHandler::Init() {
  MessageLoopForIO::current()->RegisterIOHandler(file_.Get(), this);

  DWORD read;
  EXPECT_FALSE(ReadFile(file_.Get(), buffer_, size(), &read, context()));
  EXPECT_EQ(static_cast<DWORD>(ERROR_IO_PENDING), GetLastError());
  if (wait_)
    WaitForIO();
}

void TestIOHandler::OnIOCompleted(MessageLoopForIO::IOContext* context,
                                  DWORD bytes_transfered, DWORD error) {
  ASSERT_TRUE(context == &context_);
  ASSERT_TRUE(SetEvent(signal_));
}

void TestIOHandler::WaitForIO() {
  EXPECT_TRUE(MessageLoopForIO::current()->WaitForIOCompletion(300, this));
  EXPECT_TRUE(MessageLoopForIO::current()->WaitForIOCompletion(400, this));
}

void RunTest_IOHandler() {
  win::ScopedHandle callback_called(CreateEvent(NULL, TRUE, FALSE, NULL));
  ASSERT_TRUE(callback_called.IsValid());

  const wchar_t* kPipeName = L"\\\\.\\pipe\\iohandler_pipe";
  win::ScopedHandle server(
      CreateNamedPipe(kPipeName, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  ASSERT_TRUE(server.IsValid());

  Thread thread("IOHandler test");
  Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ASSERT_TRUE(thread.StartWithOptions(options));

  TestIOHandler handler(kPipeName, callback_called.Get(), false);
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestIOHandler::Init, Unretained(&handler)));
  // Make sure the thread runs and sleeps for lack of work.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));

  const char buffer[] = "Hello there!";
  DWORD written;
  EXPECT_TRUE(WriteFile(server.Get(), buffer, sizeof(buffer), &written, NULL));

  DWORD result = WaitForSingleObject(callback_called.Get(), 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

void RunTest_WaitForIO() {
  win::ScopedHandle callback1_called(
      CreateEvent(NULL, TRUE, FALSE, NULL));
  win::ScopedHandle callback2_called(
      CreateEvent(NULL, TRUE, FALSE, NULL));
  ASSERT_TRUE(callback1_called.IsValid());
  ASSERT_TRUE(callback2_called.IsValid());

  const wchar_t* kPipeName1 = L"\\\\.\\pipe\\iohandler_pipe1";
  const wchar_t* kPipeName2 = L"\\\\.\\pipe\\iohandler_pipe2";
  win::ScopedHandle server1(
      CreateNamedPipe(kPipeName1, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  win::ScopedHandle server2(
      CreateNamedPipe(kPipeName2, PIPE_ACCESS_OUTBOUND, 0, 1, 0, 0, 0, NULL));
  ASSERT_TRUE(server1.IsValid());
  ASSERT_TRUE(server2.IsValid());

  Thread thread("IOHandler test");
  Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ASSERT_TRUE(thread.StartWithOptions(options));

  TestIOHandler handler1(kPipeName1, callback1_called.Get(), false);
  TestIOHandler handler2(kPipeName2, callback2_called.Get(), true);
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestIOHandler::Init, Unretained(&handler1)));
  // TODO(ajwong): Do we really need such long Sleeps in this function?
  // Make sure the thread runs and sleeps for lack of work.
  TimeDelta delay = TimeDelta::FromMilliseconds(100);
  PlatformThread::Sleep(delay);
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestIOHandler::Init, Unretained(&handler2)));
  PlatformThread::Sleep(delay);

  // At this time handler1 is waiting to be called, and the thread is waiting
  // on the Init method of handler2, filtering only handler2 callbacks.

  const char buffer[] = "Hello there!";
  DWORD written;
  EXPECT_TRUE(WriteFile(server1.Get(), buffer, sizeof(buffer), &written, NULL));
  PlatformThread::Sleep(2 * delay);
  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            WaitForSingleObject(callback1_called.Get(), 0))
      << "handler1 has not been called";

  EXPECT_TRUE(WriteFile(server2.Get(), buffer, sizeof(buffer), &written, NULL));

  HANDLE objects[2] = { callback1_called.Get(), callback2_called.Get() };
  DWORD result = WaitForMultipleObjects(2, objects, TRUE, 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

#endif  // defined(OS_WIN)

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that message loops work properly in all configurations.  Of course, in some
// cases, a unit test may only be for a particular type of loop.

namespace {

class MessageLoopTypedTest
    : public ::testing::TestWithParam<MessageLoop::Type> {
 public:
  MessageLoopTypedTest() = default;
  ~MessageLoopTypedTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<MessageLoop::Type> param_info) {
    switch (param_info.param) {
      case MessageLoop::TYPE_DEFAULT:
        return "Default";
      case MessageLoop::TYPE_IO:
        return "IO";
      case MessageLoop::TYPE_UI:
        return "UI";
      default:
        NOTREACHED();
        return "Unknown";
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageLoopTypedTest);
};

}  // namespace

TEST_P(MessageLoopTypedTest, PostTask) {
  MessageLoop loop(GetParam());
  // Add tests to message loop
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a"), b("b"), c("c"), d("d");
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test0, foo));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test1Ptr, foo, &b));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&Foo::Test1Int, foo, 100));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Ptr, foo, &a, &c));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Mixed, foo, a, &d));
  // After all tests, post a message that will shut down the message loop
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RunLoop::QuitCurrentWhenIdleDeprecated));

  // Now kick things off
  RunLoop().Run();

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_Basic) {
  MessageLoop loop(GetParam());

  // Test that PostDelayedTask results in a delayed task.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 1;
  TimeTicks run_time;

  TimeTicks time_before_run = TimeTicks::Now();
  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks), kDelay);
  RunLoop().Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);
  EXPECT_LT(kDelay, time_after_run - time_before_run);
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_InDelayOrder) {
  MessageLoop loop(GetParam());

  // Test that two tasks with different delays run in the right order.
  int num_tasks = 2;
  TimeTicks run_time1, run_time2;

  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromMilliseconds(200));
  // If we get a large pause in execution (due to a context switch) here, this
  // test could fail.
  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 < run_time1);
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_InPostOrder) {
  MessageLoop loop(GetParam());

  // Test that two tasks with the same delay run in the order in which they
  // were posted.
  //
  // NOTE: This is actually an approximate test since the API only takes a
  // "delay" parameter, so we are not exactly simulating two tasks that get
  // posted at the exact same time.  It would be nice if the API allowed us to
  // specify the desired run time.

  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  int num_tasks = 2;
  TimeTicks run_time1, run_time2;

  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks), kDelay);
  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks), kDelay);

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time1 < run_time2);
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_InPostOrder_2) {
  MessageLoop loop(GetParam());

  // Test that a delayed task still runs after a normal tasks even if the
  // normal tasks take a long time to run.

  const TimeDelta kPause = TimeDelta::FromMilliseconds(50);

  int num_tasks = 2;
  TimeTicks run_time;

  loop.task_runner()->PostTask(FROM_HERE,
                               BindOnce(&SlowFunc, kPause, &num_tasks));
  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  TimeTicks time_before_run = TimeTicks::Now();
  RunLoop().Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);

  EXPECT_LT(kPause, time_after_run - time_before_run);
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_InPostOrder_3) {
  MessageLoop loop(GetParam());

  // Test that a delayed task still runs after a pile of normal tasks.  The key
  // difference between this test and the previous one is that here we return
  // the MessageLoop a lot so we give the MessageLoop plenty of opportunities
  // to maybe run the delayed task.  It should know not to do so until the
  // delayed task's delay has passed.

  int num_tasks = 11;
  TimeTicks run_time1, run_time2;

  // Clutter the ML with tasks.
  for (int i = 1; i < num_tasks; ++i)
    loop.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks));

  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(1));

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 > run_time1);
}

TEST_P(MessageLoopTypedTest, PostDelayedTask_SharedTimer) {
  MessageLoop loop(GetParam());

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  TimeTicks run_time1, run_time2;

  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks),
      TimeDelta::FromSeconds(1000));
  loop.task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks),
      TimeDelta::FromMilliseconds(10));

  TimeTicks start_time = TimeTicks::Now();

  RunLoop().Run();
  EXPECT_EQ(0, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = TimeTicks::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time1.is_null());
  EXPECT_FALSE(run_time2.is_null());
}

namespace {

// This is used to inject a test point for recording the destructor calls for
// Closure objects send to MessageLoop::PostTask(). It is awkward usage since we
// are trying to hook the actual destruction, which is not a common operation.
class RecordDeletionProbe : public RefCounted<RecordDeletionProbe> {
 public:
  RecordDeletionProbe(RecordDeletionProbe* post_on_delete, bool* was_deleted)
      : post_on_delete_(post_on_delete), was_deleted_(was_deleted) {}
  void Run() {}

 private:
  friend class RefCounted<RecordDeletionProbe>;

  ~RecordDeletionProbe() {
    *was_deleted_ = true;
    if (post_on_delete_.get())
      ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, BindOnce(&RecordDeletionProbe::Run, post_on_delete_));
  }

  scoped_refptr<RecordDeletionProbe> post_on_delete_;
  bool* was_deleted_;
};

}  // namespace

/* TODO(darin): MessageLoop does not support deleting all tasks in the */
/* destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(MessageLoopTypedTest, DISABLED_EnsureDeletion) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  {
    MessageLoop loop(GetParam());
    loop.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordDeletionProbe::Run,
                            new RecordDeletionProbe(NULL, &a_was_deleted)));
    // TODO(ajwong): Do we really need 1000ms here?
    loop.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&RecordDeletionProbe::Run,
                 new RecordDeletionProbe(NULL, &b_was_deleted)),
        TimeDelta::FromMilliseconds(1000));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
}

/* TODO(darin): MessageLoop does not support deleting all tasks in the */
/* destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(MessageLoopTypedTest, DISABLED_EnsureDeletion_Chain) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  bool c_was_deleted = false;
  {
    MessageLoop loop(GetParam());
    // The scoped_refptr for each of the below is held either by the chained
    // RecordDeletionProbe, or the bound RecordDeletionProbe::Run() callback.
    RecordDeletionProbe* a = new RecordDeletionProbe(NULL, &a_was_deleted);
    RecordDeletionProbe* b = new RecordDeletionProbe(a, &b_was_deleted);
    RecordDeletionProbe* c = new RecordDeletionProbe(b, &c_was_deleted);
    loop.task_runner()->PostTask(FROM_HERE,
                                 BindOnce(&RecordDeletionProbe::Run, c));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
  EXPECT_TRUE(c_was_deleted);
}

namespace {

void NestingFunc(int* depth) {
  if (*depth > 0) {
    *depth -= 1;
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            BindOnce(&NestingFunc, depth));

    MessageLoop::current()->SetNestableTasksAllowed(true);
    RunLoop().Run();
  }
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

TEST_P(MessageLoopTypedTest, Nesting) {
  MessageLoop loop(GetParam());

  int depth = 50;
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&NestingFunc, &depth));
  RunLoop().Run();
  EXPECT_EQ(depth, 0);
}

TEST_P(MessageLoopTypedTest, RecursiveDenial1) {
  MessageLoop loop(GetParam());

  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 1, 2, false));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 2, 2, false));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&QuitFunc, &order, 3));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

namespace {

void RecursiveSlowFunc(TaskList* order,
                       int cookie,
                       int depth,
                       bool is_reentrant) {
  RecursiveFunc(order, cookie, depth, is_reentrant);
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
}

void OrderedFunc(TaskList* order, int cookie) {
  order->RecordStart(ORDERED, cookie);
  order->RecordEnd(ORDERED, cookie);
}

}  // namespace

TEST_P(MessageLoopTypedTest, RecursiveDenial3) {
  MessageLoop loop(GetParam());

  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveSlowFunc, &order, 1, 2, false));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveSlowFunc, &order, 2, 2, false));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 3),
      TimeDelta::FromMilliseconds(5));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 4),
      TimeDelta::FromMilliseconds(5));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(16U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(5), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(7), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 4, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 4, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(14), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(15), TaskItem(RECURSIVE, 2, false));
}

TEST_P(MessageLoopTypedTest, RecursiveSupport1) {
  MessageLoop loop(GetParam());

  TaskList order;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 1, 2, true));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 2, 2, true));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&QuitFunc, &order, 3));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

// Tests that non nestable tasks run in FIFO if there are no nested loops.
TEST_P(MessageLoopTypedTest, NonNestableWithNoNesting) {
  MessageLoop loop(GetParam());

  TaskList order;

  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&QuitFunc, &order, 3));
  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(6U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
}

namespace {

void FuncThatPumps(TaskList* order, int cookie) {
  order->RecordStart(PUMPS, cookie);
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    RunLoop().RunUntilIdle();
  }
  order->RecordEnd(PUMPS, cookie);
}

void SleepFunc(TaskList* order, int cookie, TimeDelta delay) {
  order->RecordStart(SLEEP, cookie);
  PlatformThread::Sleep(delay);
  order->RecordEnd(SLEEP, cookie);
}

}  // namespace

// Tests that non nestable tasks don't run when there's code in the call stack.
TEST_P(MessageLoopTypedTest, NonNestableDelayedInNestedLoop) {
  MessageLoop loop(GetParam());

  TaskList order;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatPumps, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&SleepFunc, &order, 4, TimeDelta::FromMilliseconds(50)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 5));
  ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 6));

  RunLoop().Run();

  // FIFO order.
  ASSERT_EQ(12U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(PUMPS, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(3), TaskItem(SLEEP, 4, true));
  EXPECT_EQ(order.Get(4), TaskItem(SLEEP, 4, false));
  EXPECT_EQ(order.Get(5), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(7), TaskItem(PUMPS, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 6, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 6, false));
}

namespace {

void FuncThatRuns(TaskList* order, int cookie, RunLoop* run_loop) {
  order->RecordStart(RUNS, cookie);
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    run_loop->Run();
  }
  order->RecordEnd(RUNS, cookie);
}

void FuncThatQuitsNow() {
  base::RunLoop::QuitCurrentDeprecated();
}

}  // namespace

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(MessageLoopTypedTest, QuitNow) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 1, Unretained(&run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 4));  // never runs

  RunLoop().Run();

  ASSERT_EQ(6U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(MessageLoopTypedTest, RunLoopQuitTop) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(MessageLoopTypedTest, RunLoopQuitNested) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Quits current loop and immediately runs a nested loop.
void QuitAndRunNestedLoop(TaskList* order,
                          int cookie,
                          RunLoop* outer_run_loop,
                          RunLoop* nested_run_loop) {
  order->RecordStart(RUNS, cookie);
  outer_run_loop->Quit();
  nested_run_loop->Run();
  order->RecordEnd(RUNS, cookie);
}

// Test that we can run nested loop after quitting the current one.
TEST_P(MessageLoopTypedTest, RunLoopNestedAfterQuit) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&QuitAndRunNestedLoop, &order, 1, &outer_run_loop,
                          &nested_run_loop));

  outer_run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(MessageLoopTypedTest, RunLoopQuitBogus) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;
  RunLoop bogus_run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          bogus_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(MessageLoopTypedTest, RunLoopQuitDeep) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_loop1;
  RunLoop nested_loop2;
  RunLoop nested_loop3;
  RunLoop nested_loop4;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_loop1)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 2, Unretained(&nested_loop2)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 3, Unretained(&nested_loop3)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 4, Unretained(&nested_loop4)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 5));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          outer_run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 6));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop1.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 7));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop2.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 8));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop3.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 9));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          nested_loop4.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 10));

  outer_run_loop.Run();

  ASSERT_EQ(18U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works before RunWithID.
TEST_P(MessageLoopTypedTest, RunLoopQuitOrderBefore) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop run_loop;

  run_loop.Quit();

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));  // never runs
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow));  // never runs

  run_loop.Run();

  ASSERT_EQ(0U, order.Size());
}

// Tests RunLoopQuit works during RunWithID.
TEST_P(MessageLoopTypedTest, RunLoopQuitOrderDuring) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 1));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, run_loop.QuitClosure());
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));  // never runs
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow));  // never runs

  run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works after RunWithID.
TEST_P(MessageLoopTypedTest, RunLoopQuitOrderAfter) {
  MessageLoop loop(GetParam());

  TaskList order;

  RunLoop run_loop;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 1, Unretained(&run_loop)));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 2));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 3));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, run_loop.QuitClosure());  // has no affect
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&OrderedFunc, &order, 4));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          BindOnce(&FuncThatQuitsNow));

  RunLoop outer_run_loop;
  outer_run_loop.Run();

  ASSERT_EQ(8U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// There was a bug in the MessagePumpGLib where posting tasks recursively
// caused the message loop to hang, due to the buffer of the internal pipe
// becoming full. Test all MessageLoop types to ensure this issue does not
// exist in other MessagePumps.
//
// On Linux, the pipe buffer size is 64KiB by default. The bug caused one
// byte accumulated in the pipe per two posts, so we should repeat 128K
// times to reproduce the bug.
TEST_P(MessageLoopTypedTest, RecursivePosts) {
  const int kNumTimes = 1 << 17;
  MessageLoop loop(GetParam());
  loop.task_runner()->PostTask(FROM_HERE,
                               BindOnce(&PostNTasksThenQuit, kNumTimes));
  RunLoop().Run();
}

INSTANTIATE_TEST_CASE_P(,
                        MessageLoopTypedTest,
                        ::testing::Values(MessageLoop::TYPE_DEFAULT,
                                          MessageLoop::TYPE_IO,
                                          MessageLoop::TYPE_UI),
                        MessageLoopTypedTest::ParamInfoToString);

#if defined(OS_WIN)
TEST(MessageLoopTest, PostDelayedTask_SharedTimer_SubPump) {
  RunTest_PostDelayedTask_SharedTimer_SubPump();
}

// This test occasionally hangs. See http://crbug.com/44567.
TEST(MessageLoopTest, DISABLED_RecursiveDenial2) {
  RunTest_RecursiveDenial2(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveSupport2) {
  // This test requires a UI loop.
  RunTest_RecursiveSupport2(MessageLoop::TYPE_UI);
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, TaskObserver) {
  const int kNumPosts = 6;
  DummyTaskObserver observer(kNumPosts);

  MessageLoop loop;
  loop.AddTaskObserver(&observer);
  loop.task_runner()->PostTask(FROM_HERE,
                               BindOnce(&PostNTasksThenQuit, kNumPosts));
  RunLoop().Run();
  loop.RemoveTaskObserver(&observer);

  EXPECT_EQ(kNumPosts, observer.num_tasks_started());
  EXPECT_EQ(kNumPosts, observer.num_tasks_processed());
}

#if defined(OS_WIN)
TEST(MessageLoopTest, IOHandler) {
  RunTest_IOHandler();
}

TEST(MessageLoopTest, WaitForIO) {
  RunTest_WaitForIO();
}

TEST(MessageLoopTest, HighResolutionTimer) {
  MessageLoop message_loop;
  Time::EnableHighResolutionTimer(true);

  constexpr TimeDelta kFastTimer = TimeDelta::FromMilliseconds(5);
  constexpr TimeDelta kSlowTimer = TimeDelta::FromMilliseconds(100);

  {
    // Post a fast task to enable the high resolution timers.
    RunLoop run_loop;
    message_loop.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](RunLoop* run_loop) {
              EXPECT_TRUE(Time::IsHighResolutionTimerInUse());
              run_loop->QuitWhenIdle();
            },
            &run_loop),
        kFastTimer);
    run_loop.Run();
  }
  EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
  {
    // Check that a slow task does not trigger the high resolution logic.
    RunLoop run_loop;
    message_loop.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](RunLoop* run_loop) {
              EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
              run_loop->QuitWhenIdle();
            },
            &run_loop),
        kSlowTimer);
    run_loop.Run();
  }
  Time::EnableHighResolutionTimer(false);
  Time::ResetHighResolutionTimerUsage();
}

#endif  // defined(OS_WIN)

namespace {
// Inject a test point for recording the destructor calls for Closure objects
// send to MessageLoop::PostTask(). It is awkward usage since we are trying to
// hook the actual destruction, which is not a common operation.
class DestructionObserverProbe :
  public RefCounted<DestructionObserverProbe> {
 public:
  DestructionObserverProbe(bool* task_destroyed,
                           bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {
  }
  virtual void Run() {
    // This task should never run.
    ADD_FAILURE();
  }
 private:
  friend class RefCounted<DestructionObserverProbe>;

  virtual ~DestructionObserverProbe() {
    EXPECT_FALSE(*destruction_observer_called_);
    *task_destroyed_ = true;
  }

  bool* task_destroyed_;
  bool* destruction_observer_called_;
};

class MLDestructionObserver : public MessageLoop::DestructionObserver {
 public:
  MLDestructionObserver(bool* task_destroyed, bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called),
        task_destroyed_before_message_loop_(false) {
  }
  void WillDestroyCurrentMessageLoop() override {
    task_destroyed_before_message_loop_ = *task_destroyed_;
    *destruction_observer_called_ = true;
  }
  bool task_destroyed_before_message_loop() const {
    return task_destroyed_before_message_loop_;
  }
 private:
  bool* task_destroyed_;
  bool* destruction_observer_called_;
  bool task_destroyed_before_message_loop_;
};

}  // namespace

TEST(MessageLoopTest, DestructionObserverTest) {
  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  MessageLoop* loop = new MessageLoop;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  bool task_destroyed = false;
  bool destruction_observer_called = false;

  MLDestructionObserver observer(&task_destroyed, &destruction_observer_called);
  loop->AddDestructionObserver(&observer);
  loop->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&DestructionObserverProbe::Run,
               new DestructionObserverProbe(&task_destroyed,
                                            &destruction_observer_called)),
      kDelay);
  delete loop;
  EXPECT_TRUE(observer.task_destroyed_before_message_loop());
  // The task should have been destroyed when we deleted the loop.
  EXPECT_TRUE(task_destroyed);
  EXPECT_TRUE(destruction_observer_called);
}


// Verify that MessageLoop sets ThreadMainTaskRunner::current() and it
// posts tasks on that message loop.
TEST(MessageLoopTest, ThreadMainTaskRunner) {
  MessageLoop loop;

  scoped_refptr<Foo> foo(new Foo());
  std::string a("a");
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));

  // Post quit task;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&RunLoop::QuitCurrentWhenIdleDeprecated));

  // Now kick things off
  RunLoop().Run();

  EXPECT_EQ(foo->test_count(), 1);
  EXPECT_EQ(foo->result(), "a");
}

TEST(MessageLoopTest, IsType) {
  MessageLoop loop(MessageLoop::TYPE_UI);
  EXPECT_TRUE(loop.IsType(MessageLoop::TYPE_UI));
  EXPECT_FALSE(loop.IsType(MessageLoop::TYPE_IO));
  EXPECT_FALSE(loop.IsType(MessageLoop::TYPE_DEFAULT));
}

#if defined(OS_WIN)
void EmptyFunction() {}

void PostMultipleTasks() {
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          base::BindOnce(&EmptyFunction));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          base::BindOnce(&EmptyFunction));
}

static const int kSignalMsg = WM_USER + 2;

void PostWindowsMessage(HWND message_hwnd) {
  PostMessage(message_hwnd, kSignalMsg, 0, 2);
}

void EndTest(bool* did_run, HWND hwnd) {
  *did_run = true;
  PostMessage(hwnd, WM_CLOSE, 0, 0);
}

int kMyMessageFilterCode = 0x5002;

LRESULT CALLBACK TestWndProcThunk(HWND hwnd, UINT message,
                                  WPARAM wparam, LPARAM lparam) {
  if (message == WM_CLOSE)
    EXPECT_TRUE(DestroyWindow(hwnd));
  if (message != kSignalMsg)
    return DefWindowProc(hwnd, message, wparam, lparam);

  switch (lparam) {
  case 1:
    // First, we post a task that will post multiple no-op tasks to make sure
    // that the pump's incoming task queue does not become empty during the
    // test.
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::BindOnce(&PostMultipleTasks));
    // Next, we post a task that posts a windows message to trigger the second
    // stage of the test.
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PostWindowsMessage, hwnd));
    break;
  case 2:
    // Since we're about to enter a modal loop, tell the message loop that we
    // intend to nest tasks.
    MessageLoop::current()->SetNestableTasksAllowed(true);
    bool did_run = false;
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&EndTest, &did_run, hwnd));
    // Run a nested windows-style message loop and verify that our task runs. If
    // it doesn't, then we'll loop here until the test times out.
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
      if (!CallMsgFilter(&msg, kMyMessageFilterCode))
        DispatchMessage(&msg);
      // If this message is a WM_CLOSE, explicitly exit the modal loop. Posting
      // a WM_QUIT should handle this, but unfortunately MessagePumpWin eats
      // WM_QUIT messages even when running inside a modal loop.
      if (msg.message == WM_CLOSE)
        break;
    }
    EXPECT_TRUE(did_run);
    RunLoop::QuitCurrentWhenIdleDeprecated();
    break;
  }
  return 0;
}

TEST(MessageLoopTest, AlwaysHaveUserMessageWhenNesting) {
  MessageLoop loop(MessageLoop::TYPE_UI);
  HINSTANCE instance = CURRENT_MODULE();
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = TestWndProcThunk;
  wc.hInstance = instance;
  wc.lpszClassName = L"MessageLoopTest_HWND";
  ATOM atom = RegisterClassEx(&wc);
  ASSERT_TRUE(atom);

  HWND message_hwnd = CreateWindow(MAKEINTATOM(atom), 0, 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, 0, instance, 0);
  ASSERT_TRUE(message_hwnd) << GetLastError();

  ASSERT_TRUE(PostMessage(message_hwnd, kSignalMsg, 0, 1));

  RunLoop().Run();

  ASSERT_TRUE(UnregisterClass(MAKEINTATOM(atom), instance));
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, SetTaskRunner) {
  MessageLoop loop;
  scoped_refptr<SingleThreadTaskRunner> new_runner(new TestSimpleTaskRunner());

  loop.SetTaskRunner(new_runner);
  EXPECT_EQ(new_runner, loop.task_runner());
  EXPECT_EQ(new_runner, ThreadTaskRunnerHandle::Get());
}

TEST(MessageLoopTest, OriginalRunnerWorks) {
  MessageLoop loop;
  scoped_refptr<SingleThreadTaskRunner> new_runner(new TestSimpleTaskRunner());
  scoped_refptr<SingleThreadTaskRunner> original_runner(loop.task_runner());
  loop.SetTaskRunner(new_runner);

  scoped_refptr<Foo> foo(new Foo());
  original_runner->PostTask(FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, "a"));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1, foo->test_count());
}

TEST(MessageLoopTest, DeleteUnboundLoop) {
  // It should be possible to delete an unbound message loop on a thread which
  // already has another active loop. This happens when thread creation fails.
  MessageLoop loop;
  std::unique_ptr<MessageLoop> unbound_loop(MessageLoop::CreateUnbound(
      MessageLoop::TYPE_DEFAULT, MessageLoop::MessagePumpFactoryCallback()));
  unbound_loop.reset();
  EXPECT_EQ(&loop, MessageLoop::current());
  EXPECT_EQ(loop.task_runner(), ThreadTaskRunnerHandle::Get());
}

TEST(MessageLoopTest, ThreadName) {
  {
    std::string kThreadName("foo");
    MessageLoop loop;
    PlatformThread::SetName(kThreadName);
    EXPECT_EQ(kThreadName, loop.GetThreadName());
  }

  {
    std::string kThreadName("bar");
    base::Thread thread(kThreadName);
    ASSERT_TRUE(thread.StartAndWaitForTesting());
    EXPECT_EQ(kThreadName, thread.message_loop()->GetThreadName());
  }
}

// Verify that tasks posted to and code running in the scope of the same
// MessageLoop access the same SequenceLocalStorage values.
TEST(MessageLoopTest, SequenceLocalStorageSetGet) {
  MessageLoop loop;

  SequenceLocalStorageSlot<int> slot;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&SequenceLocalStorageSlot<int>::Set, Unretained(&slot), 11));

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(
                     [](SequenceLocalStorageSlot<int>* slot) {
                       EXPECT_EQ(slot->Get(), 11);
                     },
                     &slot));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(slot.Get(), 11);
}

// Verify that tasks posted to and code running in different MessageLoops access
// different SequenceLocalStorage values.
TEST(MessageLoopTest, SequenceLocalStorageDifferentMessageLoops) {
  SequenceLocalStorageSlot<int> slot;

  {
    MessageLoop loop;
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        BindOnce(&SequenceLocalStorageSlot<int>::Set, Unretained(&slot), 11));

    RunLoop().RunUntilIdle();
    EXPECT_EQ(slot.Get(), 11);
  }

  MessageLoop loop;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(
                     [](SequenceLocalStorageSlot<int>* slot) {
                       EXPECT_NE(slot->Get(), 11);
                     },
                     &slot));

  RunLoop().RunUntilIdle();
  EXPECT_NE(slot.Get(), 11);
}

}  // namespace base
