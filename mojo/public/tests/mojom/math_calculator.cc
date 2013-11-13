// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./math_calculator.h"

#include "mojo/public/bindings/lib/message_builder.h"
#include "./math_calculator_internal.h"

namespace math {
namespace {

#pragma pack(push, 1)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
const uint32_t kCalculator_Clear_Name = 0;
class Calculator_Clear_Params {
 public:
  static Calculator_Clear_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Calculator_Clear_Params)))
        Calculator_Clear_Params();
  }





 private:
  friend class mojo::internal::ObjectTraits<Calculator_Clear_Params>;

  Calculator_Clear_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;

};
MOJO_COMPILE_ASSERT(sizeof(Calculator_Clear_Params) == 8,
                    bad_sizeof_Calculator_Clear_Params);

const uint32_t kCalculator_Add_Name = 1;
class Calculator_Add_Params {
 public:
  static Calculator_Add_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Calculator_Add_Params)))
        Calculator_Add_Params();
  }

  void set_value(double value) { value_ = value; }

  double value() const { return value_; }

 private:
  friend class mojo::internal::ObjectTraits<Calculator_Add_Params>;

  Calculator_Add_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;
  double value_;
};
MOJO_COMPILE_ASSERT(sizeof(Calculator_Add_Params) == 16,
                    bad_sizeof_Calculator_Add_Params);

const uint32_t kCalculator_Multiply_Name = 2;
class Calculator_Multiply_Params {
 public:
  static Calculator_Multiply_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Calculator_Multiply_Params)))
        Calculator_Multiply_Params();
  }

  void set_value(double value) { value_ = value; }

  double value() const { return value_; }

 private:
  friend class mojo::internal::ObjectTraits<Calculator_Multiply_Params>;

  Calculator_Multiply_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;
  double value_;
};
MOJO_COMPILE_ASSERT(sizeof(Calculator_Multiply_Params) == 16,
                    bad_sizeof_Calculator_Multiply_Params);

const uint32_t kCalculatorUI_Output_Name = 0;
class CalculatorUI_Output_Params {
 public:
  static CalculatorUI_Output_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(CalculatorUI_Output_Params)))
        CalculatorUI_Output_Params();
  }

  void set_value(double value) { value_ = value; }

  double value() const { return value_; }

 private:
  friend class mojo::internal::ObjectTraits<CalculatorUI_Output_Params>;

  CalculatorUI_Output_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }

  mojo::internal::StructHeader _header_;
  double value_;
};
MOJO_COMPILE_ASSERT(sizeof(CalculatorUI_Output_Params) == 16,
                    bad_sizeof_CalculatorUI_Output_Params);

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma pack(pop)

}  // namespace



CalculatorProxy::CalculatorProxy(mojo::MessageReceiver* receiver)
    : receiver_(receiver) {
}

void CalculatorProxy::Clear() {
  size_t payload_size =
      mojo::internal::Align(sizeof(Calculator_Clear_Params));


  mojo::MessageBuilder builder(kCalculator_Clear_Name, payload_size);

  Calculator_Clear_Params* params =
      Calculator_Clear_Params::New(builder.buffer());



  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  message.data = builder.Finish();

  receiver_->Accept(&message);
}


void CalculatorProxy::Add(double value) {
  size_t payload_size =
      mojo::internal::Align(sizeof(Calculator_Add_Params));


  mojo::MessageBuilder builder(kCalculator_Add_Name, payload_size);

  Calculator_Add_Params* params =
      Calculator_Add_Params::New(builder.buffer());

  params->set_value(value);

  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  message.data = builder.Finish();

  receiver_->Accept(&message);
}


void CalculatorProxy::Multiply(double value) {
  size_t payload_size =
      mojo::internal::Align(sizeof(Calculator_Multiply_Params));


  mojo::MessageBuilder builder(kCalculator_Multiply_Name, payload_size);

  Calculator_Multiply_Params* params =
      Calculator_Multiply_Params::New(builder.buffer());

  params->set_value(value);

  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  message.data = builder.Finish();

  receiver_->Accept(&message);
}


bool CalculatorStub::Accept(mojo::Message* message) {
  switch (message->data->header.name) {
    case kCalculator_Clear_Name: {
      Calculator_Clear_Params* params =
          reinterpret_cast<Calculator_Clear_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;
      Clear();
      break;
    }

    case kCalculator_Add_Name: {
      Calculator_Add_Params* params =
          reinterpret_cast<Calculator_Add_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;
      Add(params->value());
      break;
    }

    case kCalculator_Multiply_Name: {
      Calculator_Multiply_Params* params =
          reinterpret_cast<Calculator_Multiply_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;
      Multiply(params->value());
      break;
    }

  }
  return true;
}


CalculatorUIProxy::CalculatorUIProxy(mojo::MessageReceiver* receiver)
    : receiver_(receiver) {
}

void CalculatorUIProxy::Output(double value) {
  size_t payload_size =
      mojo::internal::Align(sizeof(CalculatorUI_Output_Params));


  mojo::MessageBuilder builder(kCalculatorUI_Output_Name, payload_size);

  CalculatorUI_Output_Params* params =
      CalculatorUI_Output_Params::New(builder.buffer());

  params->set_value(value);

  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  message.data = builder.Finish();

  receiver_->Accept(&message);
}


bool CalculatorUIStub::Accept(mojo::Message* message) {
  switch (message->data->header.name) {
    case kCalculatorUI_Output_Name: {
      CalculatorUI_Output_Params* params =
          reinterpret_cast<CalculatorUI_Output_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;
      Output(params->value());
      break;
    }

  }
  return true;
}


}  // namespace math

namespace mojo {
namespace internal {



template <>
class ObjectTraits<math::Calculator_Clear_Params> {
 public:
  static void EncodePointersAndHandles(
      math::Calculator_Clear_Params* params,
      std::vector<Handle>* handles) {


  }

  static bool DecodePointersAndHandles(
      math::Calculator_Clear_Params* params,
      const Message& message) {


    return true;
  }
};

template <>
class ObjectTraits<math::Calculator_Add_Params> {
 public:
  static void EncodePointersAndHandles(
      math::Calculator_Add_Params* params,
      std::vector<Handle>* handles) {


  }

  static bool DecodePointersAndHandles(
      math::Calculator_Add_Params* params,
      const Message& message) {


    return true;
  }
};

template <>
class ObjectTraits<math::Calculator_Multiply_Params> {
 public:
  static void EncodePointersAndHandles(
      math::Calculator_Multiply_Params* params,
      std::vector<Handle>* handles) {


  }

  static bool DecodePointersAndHandles(
      math::Calculator_Multiply_Params* params,
      const Message& message) {


    return true;
  }
};

template <>
class ObjectTraits<math::CalculatorUI_Output_Params> {
 public:
  static void EncodePointersAndHandles(
      math::CalculatorUI_Output_Params* params,
      std::vector<Handle>* handles) {


  }

  static bool DecodePointersAndHandles(
      math::CalculatorUI_Output_Params* params,
      const Message& message) {


    return true;
  }
};


}  // namespace internal
}  // namespace mojo
