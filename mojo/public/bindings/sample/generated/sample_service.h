// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_

#include "mojo/public/bindings/lib/bindings.h"

namespace sample {

class Bar {
 public:
  static Bar* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Bar))) Bar();
  }

  void set_alpha(uint8_t alpha) { d_.alpha = alpha; }
  void set_beta(uint8_t beta) { d_.beta = beta; }
  void set_gamma(uint8_t gamma) { d_.gamma = gamma; }

  uint8_t alpha() const { return d_.alpha; }
  uint8_t beta() const { return d_.beta; }
  uint8_t gamma() const { return d_.gamma; }

 private:
  friend class mojo::internal::ObjectTraits<Bar>;

  Bar() {
    header_.num_bytes = sizeof(*this);
    header_.num_fields = 3;
  }
  ~Bar();  // NOT IMPLEMENTED

  mojo::internal::StructHeader header_;
  struct Data {
    uint8_t alpha;
    uint8_t beta;
    uint8_t gamma;
  } d_;
};

class Foo {
 public:
  static Foo* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Foo))) Foo();
  }

  void set_x(int32_t x) { d_.x = x; }
  void set_y(int32_t y) { d_.y = y; }
  void set_a(bool a) { d_.a = a; }
  void set_b(bool b) { d_.b = b; }
  void set_c(bool c) { d_.c = c; }
  void set_bar(Bar* bar) { d_.bar.ptr = bar; }
  void set_data(mojo::Array<uint8_t>* data) { d_.data.ptr = data; }
  void set_extra_bars(mojo::Array<Bar*>* extra_bars) {
    d_.extra_bars.ptr = extra_bars;
  }
  void set_name(mojo::String* name) {
    d_.name.ptr = name;
  }
  void set_files(mojo::Array<mojo::Handle>* files) {
    d_.files.ptr = files;
  }

  int32_t x() const { return d_.x; }
  int32_t y() const { return d_.y; }
  bool a() const { return d_.a; }
  bool b() const { return d_.b; }
  bool c() const { return d_.c; }
  const Bar* bar() const { return d_.bar.ptr; }
  const mojo::Array<uint8_t>* data() const { return d_.data.ptr; }
  const mojo::Array<Bar*>* extra_bars() const {
    // NOTE: extra_bars is an optional field!
    return header_.num_fields >= 8 ? d_.extra_bars.ptr : NULL;
  }
  const mojo::String* name() const {
    // NOTE: name is also an optional field!
    return header_.num_fields >= 9 ? d_.name.ptr : NULL;
  }
  const mojo::Array<mojo::Handle>* files() const {
    // NOTE: files is also an optional field!
    return header_.num_fields >= 10 ? d_.files.ptr : NULL;
  }

 private:
  friend class mojo::internal::ObjectTraits<Foo>;

  Foo() {
    header_.num_bytes = sizeof(*this);
    header_.num_fields = 10;
  }
  ~Foo();  // NOT IMPLEMENTED

  mojo::internal::StructHeader header_;
  struct Data {
    int32_t x;
    int32_t y;
    uint32_t a : 1;
    uint32_t b : 1;
    uint32_t c : 1;
    mojo::internal::StructPointer<Bar> bar;
    mojo::internal::ArrayPointer<uint8_t> data;
    mojo::internal::ArrayPointer<Bar*> extra_bars;
    mojo::internal::StringPointer name;
    mojo::internal::ArrayPointer<mojo::Handle> files;
  } d_;
};

class Service {
 public:
  virtual void Frobinate(const Foo* foo, bool baz, mojo::Handle port) = 0;
};

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
