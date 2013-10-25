// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_

#include "mojo/public/bindings/lib/bindings.h"

namespace sample {

#pragma pack(push, 1)

class Bar {
 public:
  static Bar* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Bar))) Bar();
  }

  void set_alpha(uint8_t alpha) { alpha_ = alpha; }
  void set_beta(uint8_t beta) { beta_ = beta; }
  void set_gamma(uint8_t gamma) { gamma_ = gamma; }

  uint8_t alpha() const { return alpha_; }
  uint8_t beta() const { return beta_; }
  uint8_t gamma() const { return gamma_; }

 private:
  friend class mojo::internal::ObjectTraits<Bar>;

  Bar() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }
  ~Bar();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  uint8_t alpha_;
  uint8_t beta_;
  uint8_t gamma_;
  uint8_t _pad0_[5];
};
MOJO_COMPILE_ASSERT(sizeof(Bar) == 16, bad_sizeof_Bar);

class Foo {
 public:
  static Foo* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Foo))) Foo();
  }

  void set_x(int32_t x) { x_ = x; }
  void set_y(int32_t y) { y_ = y; }
  void set_a(bool a) { a_ = a; }
  void set_b(bool b) { b_ = b; }
  void set_c(bool c) { c_ = c; }
  void set_bar(Bar* bar) { bar_.ptr = bar; }
  void set_data(mojo::Array<uint8_t>* data) { data_.ptr = data; }
  void set_extra_bars(mojo::Array<Bar*>* extra_bars) {
    extra_bars_.ptr = extra_bars;
  }
  void set_name(mojo::String* name) {
    name_.ptr = name;
  }
  void set_files(mojo::Array<mojo::Handle>* files) {
    files_.ptr = files;
  }

  int32_t x() const { return x_; }
  int32_t y() const { return y_; }
  bool a() const { return a_; }
  bool b() const { return b_; }
  bool c() const { return c_; }
  const Bar* bar() const { return bar_.ptr; }
  const mojo::Array<uint8_t>* data() const { return data_.ptr; }
  const mojo::Array<Bar*>* extra_bars() const {
    // NOTE: extra_bars is an optional field!
    return _header_.num_fields >= 8 ? extra_bars_.ptr : NULL;
  }
  const mojo::String* name() const {
    // NOTE: name is also an optional field!
    return _header_.num_fields >= 9 ? name_.ptr : NULL;
  }
  const mojo::Array<mojo::Handle>* files() const {
    // NOTE: files is also an optional field!
    return _header_.num_fields >= 10 ? files_.ptr : NULL;
  }

 private:
  friend class mojo::internal::ObjectTraits<Foo>;

  Foo() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 10;
  }
  ~Foo();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  int32_t x_;
  int32_t y_;
  uint8_t a_ : 1;
  uint8_t b_ : 1;
  uint8_t c_ : 1;
  uint8_t _pad0_[7];
  mojo::internal::StructPointer<Bar> bar_;
  mojo::internal::ArrayPointer<uint8_t> data_;
  mojo::internal::ArrayPointer<Bar*> extra_bars_;
  mojo::internal::StringPointer name_;
  mojo::internal::ArrayPointer<mojo::Handle> files_;
};
MOJO_COMPILE_ASSERT(sizeof(Foo) == 64, bad_sizeof_Foo);

#pragma pack(pop)

class Service {
 public:
  virtual void Frobinate(const Foo* foo, bool baz, mojo::Handle port) = 0;
};

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
