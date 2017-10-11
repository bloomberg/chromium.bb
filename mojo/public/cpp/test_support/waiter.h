// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_TEST_SUPPORT_WAITER_H_
#define MOJO_PUBLIC_CPP_TEST_SUPPORT_WAITER_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"

namespace mojo {
namespace test {

namespace internal {

template <typename CallbackType, typename OutArgTuple>
struct WaiterHelper;

template <typename... InArgs, typename... OutArgs>
struct WaiterHelper<base::OnceCallback<void(InArgs...)>,
                    std::tuple<OutArgs...>> {
  static void MoveOrCopyInputs(base::OnceClosure continuation,
                               OutArgs*... outputs,
                               InArgs... inputs) {
    int ignored[] = {0, MoveOrCopy(std::forward<InArgs>(inputs), outputs)...};
    ALLOW_UNUSED_LOCAL(ignored);
    std::move(continuation).Run();
  }

 private:
  template <typename InputType>
  using InputTypeToOutputType = typename std::remove_const<
      typename std::remove_reference<InputType>::type>::type;

  template <typename InputType>
  static int MoveOrCopy(InputType&& input,
                        InputTypeToOutputType<InputType>* output) {
    *output = std::move(input);
    return 0;
  }
};

}  // namespace internal

// Waits for a mojo method callback and captures the results. Allows test code
// to be written in a synchronous style, but does not block the current thread
// like [Sync] mojo methods do.
//
// For mojom method:
//
// interface Foo {
//   DoFoo() => (int32 i, bool b);
// };
//
// TEST(FooBarTest) {
//   Waiter waiter;
//   int i = 0;
//   bool b = false;
//   interface_ptr->DoFoo(
//       waiter.CaptureNext<mojom::Foo::DoFooCallback>(&i, &b));
//   waiter.Wait();
//   EXPECT_EQ(0, i);
//   EXPECT_TRUE(b);
// }
class Waiter {
 public:
  Waiter();
  ~Waiter();

  // Stores |outputs| to be captured later. Can be called more than once.
  // Can be used with an empty argument list.
  template <typename CallbackType, typename... OutArgs>
  CallbackType CaptureNext(OutArgs*... outputs) {
    DCHECK(!loop_ || !loop_->running());
    loop_ = std::make_unique<base::RunLoop>();
    using Helper = internal::WaiterHelper<CallbackType, std::tuple<OutArgs...>>;
    return base::BindOnce(&Helper::MoveOrCopyInputs, loop_->QuitClosure(),
                          outputs...);
  }

  // Runs the run loop to wait for results.
  void Wait();

 private:
  std::unique_ptr<base::RunLoop> loop_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_TEST_SUPPORT_WAITER_H_
