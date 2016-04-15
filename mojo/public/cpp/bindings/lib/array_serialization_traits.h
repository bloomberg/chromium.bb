// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_TRAITS_H_

#include <stddef.h>
#include <string.h>  // For |memcpy()|.

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace WTF {
class String;
}

namespace mojo {
namespace internal {

template <typename ArrayType,
          typename ElementType,
          typename ElementDataType,
          bool is_union = IsUnionDataType<
              typename RemovePointer<ElementDataType>::type>::value>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename ArrayType, typename E, typename F>
struct ArraySerializer<ArrayType, E, F, false> {
  static_assert(sizeof(E) == sizeof(F), "Incorrect array serializer");
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    return sizeof(Array_Data<F>) + Align(input.size() * sizeof(E));
  }

  static void SerializeElements(ArrayType input,
                                Buffer* buf,
                                Array_Data<F>* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    if (input.size())
      memcpy(output->storage(), &input.storage()[0], input.size() * sizeof(E));
  }
  static bool DeserializeElements(Array_Data<F>* input,
                                  ArrayType* output,
                                  SerializationContext* context) {
    ArrayType result(input->size());
    if (input->size())
      memcpy(&result.front(), input->storage(), input->size() * sizeof(E));
    output->Swap(&result);
    return true;
  }
};

// Serializes and deserializes arrays of bools.
template <typename ArrayType>
struct ArraySerializer<ArrayType, bool, bool, false> {
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    return sizeof(Array_Data<bool>) + Align((input.size() + 7) / 8);
  }

  static void SerializeElements(ArrayType input,
                                Buffer* buf,
                                Array_Data<bool>* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input.size(); ++i)
      output->at(i) = input[i];
  }
  static bool DeserializeElements(Array_Data<bool>* input,
                                  ArrayType* output,
                                  SerializationContext* context) {
    ArrayType result(input->size());
    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = input->at(i);
    output->Swap(&result);
    return true;
  }
};

// Serializes and deserializes arrays of handles.
template <typename ArrayType, typename H>
struct ArraySerializer<ArrayType, ScopedHandleBase<H>, H, false> {
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    return sizeof(Array_Data<H>) + Align(input.size() * sizeof(H));
  }

  static void SerializeElements(ArrayType input,
                                Buffer* buf,
                                Array_Data<H>* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle type should not have array validate params";

    for (size_t i = 0; i < input.size(); ++i) {
      output->at(i) = input[i].release();  // Transfer ownership of the handle.
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !output->at(i).is_valid(),
          VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
          MakeMessageWithArrayIndex(
              "invalid handle in array expecting valid handles", input.size(),
              i));
    }
  }
  static bool DeserializeElements(Array_Data<H>* input,
                                  ArrayType* output,
                                  SerializationContext* context) {
    ArrayType result(input->size());
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = MakeScopedHandle(FetchAndReset(&input->at(i)));
    output->Swap(&result);
    return true;
  }
};

// This template must only apply to pointer mojo entity (strings, structs,
// arrays and maps). This is done by ensuring that WrapperTraits<S>::DataType is
// a pointer.
template <typename ArrayType, typename S>
struct ArraySerializer<
    ArrayType,
    S,
    typename EnableIf<IsPointer<typename WrapperTraits<S>::DataType>::value,
                      typename WrapperTraits<S>::DataType>::type,
    false> {
  typedef
      typename RemovePointer<typename WrapperTraits<S>::DataType>::type S_Data;
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    size_t size = sizeof(Array_Data<S_Data*>) +
                  input.size() * sizeof(StructPointer<S_Data>);
    for (size_t i = 0; i < input.size(); ++i)
      size += GetSerializedSize_(input[i], context);
    return size;
  }

  static void SerializeElements(ArrayType input,
                                Buffer* buf,
                                Array_Data<S_Data*>* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    for (size_t i = 0; i < input.size(); ++i) {
      S_Data* element;
      SerializeCaller<S>::Run(std::move(input[i]), buf, &element,
                              validate_params->element_validate_params,
                              context);
      output->at(i) = element;
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !element,
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    input.size(), i));
    }
  }
  static bool DeserializeElements(Array_Data<S_Data*>* input,
                                  ArrayType* output,
                                  SerializationContext* context) {
    bool success = true;
    ArrayType result(input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      // Note that we rely on complete deserialization taking place in order to
      // transfer ownership of all encoded handles. Therefore we don't
      // short-circuit on failure here.
      if (!Deserialize_(input->at(i), &result[i], context))
        success = false;
    }
    output->Swap(&result);
    return success;
  }

 private:
  template <typename T,
            bool is_array = IsSpecializationOf<Array, T>::value ||
                            IsSpecializationOf<WTFArray, T>::value,
            bool is_string = std::is_same<T, String>::value ||
                             std::is_same<T, WTF::String>::value>
  struct SerializeCaller {
    static void Run(T input,
                    Buffer* buf,
                    typename WrapperTraits<T>::DataType* output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      DCHECK(!validate_params)
          << "Struct type should not have array validate params";

      Serialize_(std::move(input), buf, output, context);
    }
  };

  template <typename T>
  struct SerializeCaller<T, true, false> {
    static void Run(T input,
                    Buffer* buf,
                    typename T::Data_** output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      SerializeArray_(std::move(input), buf, output, validate_params, context);
    }
  };

  template <typename T, typename U>
  struct SerializeCaller<Map<T, U>, false, false> {
    static void Run(Map<T, U> input,
                    Buffer* buf,
                    typename Map<T, U>::Data_** output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      SerializeMap_(std::move(input), buf, output, validate_params, context);
    }
  };

  template <typename T>
  struct SerializeCaller<T, false, true> {
    static void Run(const T& input,
                    Buffer* buf,
                    String_Data** output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      DCHECK(validate_params && !validate_params->element_validate_params &&
             !validate_params->element_is_nullable &&
             validate_params->expected_num_elements == 0)
          << "String type has unexpected array validate params";

      Serialize_(input, buf, output, context);
    }
  };
};

// Handles serialization and deserialization of arrays of unions.
template <typename ArrayType, typename U, typename U_Data>
struct ArraySerializer<ArrayType, U, U_Data, true> {
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    size_t size = sizeof(Array_Data<U_Data>);
    for (size_t i = 0; i < input.size(); ++i) {
      // GetSerializedSize_ will account for both the data in the union and the
      // space in the array used to hold the union.
      size += GetSerializedSize_(input[i], false, context);
    }
    return size;
  }

  static void SerializeElements(ArrayType input,
                                Buffer* buf,
                                Array_Data<U_Data>* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    for (size_t i = 0; i < input.size(); ++i) {
      U_Data* result = output->storage() + i;
      SerializeUnion_(std::move(input[i]), buf, &result, true, context);
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && output->at(i).is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    input.size(), i));
    }
  }

  static bool DeserializeElements(Array_Data<U_Data>* input,
                                  ArrayType* output,
                                  SerializationContext* context) {
    bool success = true;
    ArrayType result(input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      // Note that we rely on complete deserialization taking place in order to
      // transfer ownership of all encoded handles. Therefore we don't
      // short-circuit on failure here.
      if (!Deserialize_(&input->at(i), &result[i], context))
        success = false;
    }
    output->Swap(&result);
    return success;
  }
};

// Another layer of abstraction to switch between standard mojom type
// serializers and native-only serializers.
template <typename ArrayType,
          bool use_native =
              ShouldUseNativeSerializer<typename ArrayType::ElementType>::value>
struct ArraySerializationStrategy;

// Serialization strategy for standard mojom types. This branches further
// by choosing an ArraySerializer specialization from above.
template <typename ArrayType>
struct ArraySerializationStrategy<ArrayType, false> {
  template <class DataType>
  using Serializer =
      ArraySerializer<ArrayType, typename ArrayType::ElementType, DataType>;

  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    DCHECK(input);
    return Serializer<typename WrapperTraits<
        typename ArrayType::ElementType>::DataType>::GetSerializedSize(input,
                                                                       context);
  }

  template <typename F>
  static void Serialize(ArrayType input,
                        Buffer* buf,
                        Array_Data<F>** output,
                        const ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    DCHECK(input);
    Array_Data<F>* result = Array_Data<F>::New(input.size(), buf);
    if (result) {
      Serializer<F>::SerializeElements(std::move(input), buf, result,
                                       validate_params, context);
    }
    *output = result;
  }

  template <typename F>
  static bool Deserialize(Array_Data<F>* input,
                          ArrayType* output,
                          SerializationContext* context) {
    DCHECK(input);
    return Serializer<F>::DeserializeElements(input, output, context);
  }
};

// Serialization for arrays of native-only types, which are opaquely serialized
// as arrays of uint8_t arrays.
template <typename ArrayType>
struct ArraySerializationStrategy<ArrayType, true> {
  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    DCHECK(input);
    DCHECK_LE(input.size(), std::numeric_limits<uint32_t>::max());
    size_t size = ArrayDataTraits<NativeStruct_Data*>::GetStorageSize(
        static_cast<uint32_t>(input.size()));
    for (size_t i = 0; i < input.size(); ++i) {
      size_t element_size = GetSerializedSizeNative_(input[i], context);
      DCHECK_LT(element_size, std::numeric_limits<uint32_t>::max());
      size += static_cast<uint32_t>(element_size);
    }
    return size;
  }

  template <typename F>
  static void Serialize(ArrayType input,
                        Buffer* buf,
                        Array_Data<F>** output,
                        const ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    static_assert(std::is_same<F, NativeStruct_Data*>::value,
                  "Native-only type array must serialize into array of "
                  "NativeStruct_Data*.");
    DCHECK(input);
    DCHECK(validate_params);
    // TODO(rockot): We may want to support nullable (i.e. scoped_ptr<T>)
    // elements here.
    DCHECK(!validate_params->element_is_nullable);
    Array_Data<NativeStruct_Data*>* result =
        Array_Data<NativeStruct_Data*>::New(input.size(), buf);
    for (size_t i = 0; i < input.size(); ++i)
      SerializeNative_(input[i], buf, &result->at(i), context);
    *output = result;
  }

  template <typename F>
  static bool Deserialize(Array_Data<F>* input,
                          ArrayType* output,
                          SerializationContext* context) {
    static_assert(std::is_same<F, NativeStruct_Data*>::value,
                  "Native-only type array must deserialize from array of "
                  "NativeStruct_Data*.");
    DCHECK(input);

    ArrayType result(input->size());
    bool success = true;
    for (size_t i = 0; i < input->size(); ++i) {
      // We don't short-circuit on failure since we can't know what the native
      // type's ParamTraits' expectations are.
      if (!DeserializeNative_(input->at(i), &result[i], context))
        success = false;
    }
    output->Swap(&result);
    return success;
  }
};

template <typename ArrayType>
struct ArraySerializationImpl {
  using Strategy = ArraySerializationStrategy<ArrayType>;

  static size_t GetSerializedSize(const ArrayType& input,
                                  SerializationContext* context) {
    if (!input)
      return 0;
    return Strategy::GetSerializedSize(input, context);
  }

  template <typename F>
  static void Serialize(ArrayType input,
                        internal::Buffer* buf,
                        internal::Array_Data<F>** output,
                        const internal::ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    if (input) {
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          validate_params->expected_num_elements != 0 &&
              input.size() != validate_params->expected_num_elements,
          internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
          internal::MakeMessageWithExpectedArraySize(
              "fixed-size array has wrong number of elements", input.size(),
              validate_params->expected_num_elements));
      Strategy::template Serialize<F>(std::move(input), buf, output,
                                      validate_params, context);
    } else {
      *output = nullptr;
    }
  }

  template <typename F>
  static bool Deserialize(internal::Array_Data<F>* input,
                          ArrayType* output,
                          internal::SerializationContext* context) {
    if (input)
      return Strategy::template Deserialize<F>(input, output, context);
    *output = nullptr;
    return true;
  }
};

}  // namespace internal

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_TRAITS_H_
