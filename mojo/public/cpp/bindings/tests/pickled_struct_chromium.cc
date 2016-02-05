// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/pickled_struct_chromium.h"

#include "base/pickle.h"

namespace mojo {
namespace test {

PickledStructChromium::PickledStructChromium() {}

PickledStructChromium::PickledStructChromium(int foo, int bar)
    : foo_(foo), bar_(bar) {
}

PickledStructChromium::~PickledStructChromium() {}

}  // namespace test
}  // namespace mojo

namespace IPC {

void ParamTraits<mojo::test::PickledStructChromium>::GetSize(
    base::PickleSizer* sizer,
    const param_type& p) {
  sizer->AddInt();
  sizer->AddInt();
}

void ParamTraits<mojo::test::PickledStructChromium>::Write(
    base::Pickle* m,
    const param_type& p) {
  m->WriteInt(p.foo());
  m->WriteInt(p.bar());
}

bool ParamTraits<mojo::test::PickledStructChromium>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  int foo, bar;
  if (!iter->ReadInt(&foo) || !iter->ReadInt(&bar))
    return false;

  p->set_foo(foo);
  p->set_bar(bar);
  return true;
}

}  // namespace IPC
