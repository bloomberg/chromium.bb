// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_CHROMIUM_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_CHROMIUM_H_

#include <stddef.h>

#include <string>

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
// consumers in the greater Chromium tree.
class PickledStructChromium {
 public:
  PickledStructChromium();
  PickledStructChromium(int foo, int bar);
  ~PickledStructChromium();

  int foo() const { return foo_; }
  void set_foo(int foo) { foo_ = foo; }

  int bar() const { return bar_; }
  void set_bar(int bar) { bar_ = bar; }

  // The |baz| field should never be serialized.
  int baz() const { return baz_; }
  void set_baz(int baz) { baz_ = baz; }

 private:
  int foo_ = 0;
  int bar_ = 0;
  int baz_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PickledStructChromium);
};

}  // namespace test
}  // namespace mojo

namespace IPC {

template <>
struct ParamTraits<mojo::test::PickledStructChromium> {
  using param_type = mojo::test::PickledStructChromium;

  static void GetSize(base::PickleSizer* sizer, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l) {}
};

}  // namespace IPC

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_STRUCT_CHROMIUM_H_
