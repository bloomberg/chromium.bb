// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_

#include <string.h>

#include "mojo/public/bindings/lib/bindings_serialization.h"

namespace sample {

class Bar;
class Foo;

namespace internal {

#pragma pack(push, 1)

class Bar_Data {
 public:
  typedef Bar Wrapper;

  static Bar_Data* New(mojo::Buffer* buf);

  void set_alpha(uint8_t alpha) { alpha_ = alpha; }
  void set_beta(uint8_t beta) { beta_ = beta; }
  void set_gamma(uint8_t gamma) { gamma_ = gamma; }

  uint8_t alpha() const { return alpha_; }
  uint8_t beta() const { return beta_; }
  uint8_t gamma() const { return gamma_; }

 private:
  friend class mojo::internal::ObjectTraits<Bar_Data>;

  Bar_Data();
  ~Bar_Data();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  uint8_t alpha_;
  uint8_t beta_;
  uint8_t gamma_;
  uint8_t _pad0_[5];
};
MOJO_COMPILE_ASSERT(sizeof(Bar_Data) == 16, bad_sizeof_Bar_Data);

class Foo_Data {
 public:
  typedef Foo Wrapper;

  static Foo_Data* New(mojo::Buffer* buf);

  void set_x(int32_t x) { x_ = x; }
  void set_y(int32_t y) { y_ = y; }
  void set_a(bool a) { a_ = a; }
  void set_b(bool b) { b_ = b; }
  void set_c(bool c) { c_ = c; }
  void set_bar(Bar_Data* bar) { bar_.ptr = bar; }
  void set_data(mojo::internal::Array_Data<uint8_t>* data) { data_.ptr = data; }
  void set_extra_bars(mojo::internal::Array_Data<Bar_Data*>* extra_bars) {
    extra_bars_.ptr = extra_bars;
  }
  void set_name(mojo::internal::String_Data* name) {
    name_.ptr = name;
  }
  void set_files(mojo::internal::Array_Data<mojo::Handle>* files) {
    files_.ptr = files;
  }

  int32_t x() const { return x_; }
  int32_t y() const { return y_; }
  bool a() const { return a_; }
  bool b() const { return b_; }
  bool c() const { return c_; }
  const Bar_Data* bar() const { return bar_.ptr; }
  const mojo::internal::Array_Data<uint8_t>* data() const { return data_.ptr; }
  const mojo::internal::Array_Data<Bar_Data*>* extra_bars() const {
    // NOTE: extra_bars is an optional field!
    return _header_.num_fields >= 8 ? extra_bars_.ptr : NULL;
  }
  const mojo::internal::String_Data* name() const {
    // NOTE: name is also an optional field!
    return _header_.num_fields >= 9 ? name_.ptr : NULL;
  }
  const mojo::internal::Array_Data<mojo::Handle>* files() const {
    // NOTE: files is also an optional field!
    return _header_.num_fields >= 10 ? files_.ptr : NULL;
  }

 private:
  friend class mojo::internal::ObjectTraits<Foo_Data>;

  Foo_Data();
  ~Foo_Data();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  int32_t x_;
  int32_t y_;
  uint8_t a_ : 1;
  uint8_t b_ : 1;
  uint8_t c_ : 1;
  uint8_t _pad0_[7];
  mojo::internal::StructPointer<Bar_Data> bar_;
  mojo::internal::ArrayPointer<uint8_t> data_;
  mojo::internal::ArrayPointer<Bar_Data*> extra_bars_;
  mojo::internal::StringPointer name_;
  mojo::internal::ArrayPointer<mojo::Handle> files_;
};
MOJO_COMPILE_ASSERT(sizeof(Foo_Data) == 64, bad_sizeof_Foo_Data);

#pragma pack(pop)

}  // namespace internal
}  // namespace sample

namespace mojo {
namespace internal {

template <>
class ObjectTraits<sample::internal::Bar_Data> {
 public:
  static size_t ComputeSizeOf(const sample::internal::Bar_Data* bar);
  static sample::internal::Bar_Data* Clone(
      const sample::internal::Bar_Data* bar,
      Buffer* buf);
  static void EncodePointersAndHandles(sample::internal::Bar_Data* bar,
                                       std::vector<mojo::Handle>* handles);
  static bool DecodePointersAndHandles(sample::internal::Bar_Data* bar,
                                       const mojo::Message& message);
};

template <>
class ObjectTraits<sample::internal::Foo_Data> {
 public:
  static size_t ComputeSizeOf(const sample::internal::Foo_Data* foo);
  static sample::internal::Foo_Data* Clone(
      const sample::internal::Foo_Data* foo,
      Buffer* buf);
  static void EncodePointersAndHandles(sample::internal::Foo_Data* foo,
                                       std::vector<mojo::Handle>* handles);
  static bool DecodePointersAndHandles(sample::internal::Foo_Data* foo,
                                       const mojo::Message& message);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
