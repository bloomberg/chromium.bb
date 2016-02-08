// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_TEST_MOJO_TEST_BASE_H_
#define MOJO_EDK_TEST_MOJO_TEST_BASE_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace test {

class MojoTestBase : public testing::Test {
 public:
  MojoTestBase();
  ~MojoTestBase() override;

 protected:
  using HandlerCallback = base::Callback<void(ScopedMessagePipeHandle)>;

  class ClientController {
   public:
    ClientController(const std::string& client_name,
                     MojoTestBase* test,
                     const HandlerCallback& callback);
    ~ClientController();

    int WaitForShutdown();

   private:
    friend class MojoTestBase;

    MojoTestBase* test_;
    MultiprocessTestHelper helper_;
    bool was_shutdown_ = false;

    DISALLOW_COPY_AND_ASSIGN(ClientController);
  };

  ClientController& StartClient(
      const std::string& client_name,
      const HandlerCallback& callback);

  static void RunHandlerOnMainThread(
      std::function<void(MojoHandle, int*, const base::Closure&)> handler,
      int* expected_exit_code,
      const base::Closure& quit_closure,
      ScopedMessagePipeHandle pipe) {
    handler(pipe.get().value(), expected_exit_code, quit_closure);
  }

  static void RunHandler(
      std::function<void(MojoHandle, int*, const base::Closure&)> handler,
      int* expected_exit_code,
      const base::Closure& quit_closure,
      scoped_refptr<base::TaskRunner> task_runner,
      ScopedMessagePipeHandle pipe) {
    task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RunHandlerOnMainThread, handler,
                 base::Unretained(expected_exit_code), quit_closure,
                 base::Passed(&pipe)));
  }

  template <typename HandlerFunc>
  void StartClientWithHandler(const std::string& client_name,
                              HandlerFunc handler) {
    base::MessageLoop::ScopedNestableTaskAllower nesting(&message_loop_);
    base::RunLoop run_loop;
    int expected_exit_code;
    ClientController& c =
        StartClient(client_name,
                    base::Bind(&RunHandler, handler,
                               base::Unretained(&expected_exit_code),
                               run_loop.QuitClosure(),
                               base::ThreadTaskRunnerHandle::Get()));
    run_loop.Run();
    EXPECT_EQ(expected_exit_code, c.WaitForShutdown());
  }

  // Closes a handle and expects success.
  static void CloseHandle(MojoHandle h);

  ////// Message pipe test utilities ///////

  // Creates a new pipe, returning endpoint handles in |p0| and |p1|.
  static void CreateMessagePipe(MojoHandle* p0, MojoHandle* p1);

  // Writes a string to the pipe, transferring handles in the process.
  static void WriteMessageWithHandles(MojoHandle mp,
                                     const std::string& message,
                                     const MojoHandle* handles,
                                     uint32_t num_handles);

  // Writes a string to the pipe with no handles.
  static void WriteMessage(MojoHandle mp, const std::string& message);

  // Reads a string from the pipe, expecting to read an exact number of handles
  // in the process. Returns the read string.
  static std::string ReadMessageWithHandles(MojoHandle mp,
                                           MojoHandle* handles,
                                           uint32_t expected_num_handles);

  // Reads a string from the pipe, expecting either zero or one handles.
  // If no handle is read, |handle| will be reset.
  static std::string ReadMessageWithOptionalHandle(MojoHandle mp,
                                                  MojoHandle* handle);

  // Reads a string from the pipe, expecting to read no handles.
  // Returns the string.
  static std::string ReadMessage(MojoHandle mp);

  // Reads a string from the pipe, expecting to read no handles and exactly
  // |num_bytes| bytes, which are read into |data|.
  static void ReadMessage(MojoHandle mp, char* data, size_t num_bytes);

  // Writes |message| to |in| and expects to read it back from |out|.
  static void VerifyTransmission(MojoHandle in,
                                 MojoHandle out,
                                 const std::string& message);

  // Writes |message| to |mp| and expects to read it back from the same handle.
  static void VerifyEcho(MojoHandle mp, const std::string& message);

  //////// Shared buffer test utilities /////////

  // Creates a new shared buffer.
  static MojoHandle CreateBuffer(uint64_t size);

  // Duplicates a shared buffer to a new handle.
  static MojoHandle DuplicateBuffer(MojoHandle h);

  // Maps a buffer, writes some data into it, and unmaps it.
  static void WriteToBuffer(MojoHandle h,
                            size_t offset,
                            const base::StringPiece& s);

  // Maps a buffer, tests the value of some of its contents, and unmaps it.
  static void ExpectBufferContents(MojoHandle h,
                                   size_t offset,
                                   const base::StringPiece& s);

 private:
  friend class ClientController;

  base::MessageLoop message_loop_;
  std::vector<scoped_ptr<ClientController>> clients_;

  DISALLOW_COPY_AND_ASSIGN(MojoTestBase);
};

// Launches a new child process running the test client |client_name| connected
// to a new message pipe bound to |pipe_name|. |pipe_name| is automatically
// closed on test teardown.
#define RUN_CHILD_ON_PIPE(client_name, pipe_name)                   \
    StartClientWithHandler(                                         \
        #client_name,                                               \
        [&](MojoHandle pipe_name,                                   \
            int *expected_exit_code,                                \
            const base::Closure& quit_closure) { {

// Waits for the client to terminate and expects a return code of zero.
#define END_CHILD()               \
        }                         \
        *expected_exit_code = 0;  \
        quit_closure.Run();       \
    });

// Wait for the client to terminate with a specific return code.
#define END_CHILD_AND_EXPECT_EXIT_CODE(code) \
        }                                    \
        *expected_exit_code = code;          \
        quit_closure.Run();                  \
    });

// Use this to declare the child process's "main()" function for tests using
// MojoTestBase and MultiprocessTestHelper. It returns an |int|, which will
// will be the process's exit code (but see the comment about
// WaitForChildShutdown()).
//
// The function is defined as a subclass of |test_base| to facilitate shared
// code between test clients and to allow clients to spawn children themselves.
//
// |pipe_name| will be bound to the MojoHandle of a message pipe connected
// to the parent process (see RUN_CHILD_ON_PIPE above.) This pipe handle is
// automatically closed on test client teardown.
#define DEFINE_TEST_CLIENT_WITH_PIPE(client_name, test_base, pipe_name)     \
  class client_name##_MainFixture : public test_base {                      \
    void TestBody() override {}                                             \
   public:                                                                  \
    int Main(MojoHandle);                                                   \
  };                                                                        \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                        \
      client_name##TestChildMain,                                           \
      test::MultiprocessTestHelper::ChildSetup) {                           \
        client_name##_MainFixture test;                                     \
        return test::MultiprocessTestHelper::RunClientMain(                 \
            base::Bind(&client_name##_MainFixture::Main,                    \
                       base::Unretained(&test)));                           \
      }                                                                     \
      int client_name##_MainFixture::Main(MojoHandle pipe_name)


// This is a version of DEFINE_TEST_CLIENT_WITH_PIPE which can be used with
// gtest ASSERT/EXPECT macros.
#define DEFINE_TEST_CLIENT_TEST_WITH_PIPE(client_name, test_base, pipe_name) \
  class client_name##_MainFixture : public test_base {                       \
    void TestBody() override {}                                              \
   public:                                                                   \
    void Main(MojoHandle);                                                   \
  };                                                                         \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                         \
      client_name##TestChildMain,                                            \
      test::MultiprocessTestHelper::ChildSetup) {                            \
        client_name##_MainFixture test;                                      \
        return test::MultiprocessTestHelper::RunClientTestMain(              \
            base::Bind(&client_name##_MainFixture::Main,                     \
                       base::Unretained(&test)));                            \
      }                                                                      \
      void client_name##_MainFixture::Main(MojoHandle pipe_name)



}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_TEST_MOJO_TEST_BASE_H_
