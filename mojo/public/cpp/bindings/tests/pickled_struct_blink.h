// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_BLINK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_BLINK_H_

#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "ipc/ipc_param_traits.h"

namespace base {
class Pickle;
class PickleIterator;
class PickleSizer;
}

namespace mojo {
namespace test {

// An implementation of a hypothetical PickledStruct type specifically for
// consumers in Blink.
//
// To make things slightly more interesting, this variation of the type doesn't
// support negative values. It'll DCHECK if you try to construct it with any,
// and it will fail deserialization if negative values are decoded.
class PickledStructBlink {
 public:
  PickledStructBlink();
  PickledStructBlink(int foo, int bar);
  ~PickledStructBlink();

  int foo() const { return foo_; }
  void set_foo(int foo) {
    DCHECK_GE(foo, 0);
    foo_ = foo;
  }

  int bar() const { return bar_; }
  void set_bar(int bar) {
    DCHECK_GE(bar, 0);
    bar_ = bar;
  }

 private:
  int foo_ = 0;
  int bar_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PickledStructBlink);
};

}  // namespace test
}  // namespace mojo

namespace IPC {

template <>
struct ParamTraits<mojo::test::PickledStructBlink> {
  using param_type = mojo::test::PickledStructBlink;

  static void GetSize(base::PickleSizer* sizer, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l) {}
};

}  // namespace IPC

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_BLINK_H_
