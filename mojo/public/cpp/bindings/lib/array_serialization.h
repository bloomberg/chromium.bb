// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>  // For |memcpy()|.

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/map_serialization.h"
#include "mojo/public/cpp/bindings/lib/native_serialization.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {

template <typename E>
inline size_t GetSerializedSize_(const Array<E>& input);

template <typename E, typename F>
inline void SerializeArray_(
    Array<E> input,
    internal::Buffer* buf,
    internal::Array_Data<F>** output,
    const internal::ArrayValidateParams* validate_params);

template <typename E, typename F>
inline bool Deserialize_(internal::Array_Data<F>* input,
                         Array<E>* output,
                         internal::SerializationContext* context);

namespace internal {

template <typename E,
          typename F,
          bool is_union =
              IsUnionDataType<typename RemovePointer<F>::type>::value>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename E, typename F>
struct ArraySerializer<E, F, false> {
  static_assert(sizeof(E) == sizeof(F), "Incorrect array serializer");
  static size_t GetSerializedSize(const Array<E>& input) {
    return sizeof(Array_Data<F>) + Align(input.size() * sizeof(E));
  }

  static void SerializeElements(Array<E> input,
                                Buffer* buf,
                                Array_Data<F>* output,
                                const ArrayValidateParams* validate_params) {
    MOJO_DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    MOJO_DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    if (input.size())
      memcpy(output->storage(), &input.storage()[0], input.size() * sizeof(E));
  }
  static bool DeserializeElements(Array_Data<F>* input,
                                  Array<E>* output,
                                  SerializationContext* context) {
    std::vector<E> result(input->size());
    if (input->size())
      memcpy(&result[0], input->storage(), input->size() * sizeof(E));
    output->Swap(&result);
    return true;
  }
};

// Serializes and deserializes arrays of bools.
template <>
struct ArraySerializer<bool, bool, false> {
  static size_t GetSerializedSize(const Array<bool>& input) {
    return sizeof(Array_Data<bool>) + Align((input.size() + 7) / 8);
  }

  static void SerializeElements(Array<bool> input,
                                Buffer* buf,
                                Array_Data<bool>* output,
                                const ArrayValidateParams* validate_params) {
    MOJO_DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    MOJO_DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input.size(); ++i)
      output->at(i) = input[i];
  }
  static bool DeserializeElements(Array_Data<bool>* input,
                                  Array<bool>* output,
                                  SerializationContext* context) {
    Array<bool> result(input->size());
    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = input->at(i);
    output->Swap(&result);
    return true;
  }
};

// Serializes and deserializes arrays of handles.
template <typename H>
struct ArraySerializer<ScopedHandleBase<H>, H, false> {
  static size_t GetSerializedSize(const Array<ScopedHandleBase<H>>& input) {
    return sizeof(Array_Data<H>) + Align(input.size() * sizeof(H));
  }

  static void SerializeElements(Array<ScopedHandleBase<H>> input,
                                Buffer* buf,
                                Array_Data<H>* output,
                                const ArrayValidateParams* validate_params) {
    MOJO_DCHECK(!validate_params->element_validate_params)
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
                                  Array<ScopedHandleBase<H>>* output,
                                  SerializationContext* context) {
    Array<ScopedHandleBase<H>> result(input->size());
    for (size_t i = 0; i < input->size(); ++i)
      result.at(i) = MakeScopedHandle(FetchAndReset(&input->at(i)));
    output->Swap(&result);
    return true;
  }
};

// This template must only apply to pointer mojo entity (structs and arrays).
// This is done by ensuring that WrapperTraits<S>::DataType is a pointer.
template <typename S>
struct ArraySerializer<
    S,
    typename EnableIf<IsPointer<typename WrapperTraits<S>::DataType>::value,
                      typename WrapperTraits<S>::DataType>::type,
    false> {
  typedef
      typename RemovePointer<typename WrapperTraits<S>::DataType>::type S_Data;
  static size_t GetSerializedSize(const Array<S>& input) {
    size_t size = sizeof(Array_Data<S_Data*>) +
                  input.size() * sizeof(StructPointer<S_Data>);
    for (size_t i = 0; i < input.size(); ++i)
      size += GetSerializedSize_(input[i]);
    return size;
  }

  static void SerializeElements(Array<S> input,
                                Buffer* buf,
                                Array_Data<S_Data*>* output,
                                const ArrayValidateParams* validate_params) {
    for (size_t i = 0; i < input.size(); ++i) {
      S_Data* element;
      SerializeCaller<S>::Run(std::move(input[i]), buf, &element,
                              validate_params->element_validate_params);
      output->at(i) = element;
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !element,
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    input.size(), i));
    }
  }
  static bool DeserializeElements(Array_Data<S_Data*>* input,
                                  Array<S>* output,
                                  SerializationContext* context) {
    bool success = true;
    Array<S> result(input->size());
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
  template <typename T>
  struct SerializeCaller {
    static void Run(T input,
                    Buffer* buf,
                    typename WrapperTraits<T>::DataType* output,
                    const ArrayValidateParams* validate_params) {
      MOJO_DCHECK(!validate_params)
          << "Struct type should not have array validate params";

      Serialize_(std::move(input), buf, output);
    }
  };

  template <typename T>
  struct SerializeCaller<Array<T>> {
    static void Run(Array<T> input,
                    Buffer* buf,
                    typename Array<T>::Data_** output,
                    const ArrayValidateParams* validate_params) {
      SerializeArray_(std::move(input), buf, output, validate_params);
    }
  };

  template <typename T, typename U>
  struct SerializeCaller<Map<T, U>> {
    static void Run(Map<T, U> input,
                    Buffer* buf,
                    typename Map<T, U>::Data_** output,
                    const ArrayValidateParams* validate_params) {
      SerializeMap_(std::move(input), buf, output, validate_params);
    }
  };
};

// Handles serialization and deserialization of arrays of unions.
template <typename U, typename U_Data>
struct ArraySerializer<U, U_Data, true> {
  static size_t GetSerializedSize(const Array<U>& input) {
    size_t size = sizeof(Array_Data<U_Data>);
    for (size_t i = 0; i < input.size(); ++i) {
      // GetSerializedSize_ will account for both the data in the union and the
      // space in the array used to hold the union.
      size += GetSerializedSize_(input[i], false);
    }
    return size;
  }

  static void SerializeElements(Array<U> input,
                                Buffer* buf,
                                Array_Data<U_Data>* output,
                                const ArrayValidateParams* validate_params) {
    for (size_t i = 0; i < input.size(); ++i) {
      U_Data* result = output->storage() + i;
      SerializeUnion_(std::move(input[i]), buf, &result, true);
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && output->at(i).is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    input.size(), i));
    }
  }

  static bool DeserializeElements(Array_Data<U_Data>* input,
                                  Array<U>* output,
                                  SerializationContext* context) {
    bool success = true;
    Array<U> result(input->size());
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

// Handles serialization and deserialization of arrays of strings.
template <>
struct ArraySerializer<String, String_Data*> {
  static size_t GetSerializedSize(const Array<String>& input) {
    size_t size =
        sizeof(Array_Data<String_Data*>) + input.size() * sizeof(StringPointer);
    for (size_t i = 0; i < input.size(); ++i)
      size += GetSerializedSize_(input[i]);
    return size;
  }

  static void SerializeElements(Array<String> input,
                                Buffer* buf,
                                Array_Data<String_Data*>* output,
                                const ArrayValidateParams* validate_params) {
    MOJO_DCHECK(
        validate_params->element_validate_params &&
        !validate_params->element_validate_params->element_validate_params &&
        !validate_params->element_validate_params->element_is_nullable &&
        validate_params->element_validate_params->expected_num_elements == 0)
        << "String type has unexpected array validate params";

    for (size_t i = 0; i < input.size(); ++i) {
      String_Data* element;
      Serialize_(input[i], buf, &element);
      output->at(i) = element;
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !element,
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid strings",
                                    input.size(), i));
    }
  }
  static bool DeserializeElements(Array_Data<String_Data*>* input,
                                  Array<String>* output,
                                  SerializationContext* context) {
    Array<String> result(input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      // It's OK to short-circuit here since string data cannot contain handles.
      if (!Deserialize_(input->at(i), &result[i], context))
        return false;
    }
    output->Swap(&result);
    return true;
  }
};

// Another layer of abstraction to switch between standard mojom type
// serializers and native-only serializers.
template <typename E,
          bool use_native = ShouldUseNativeSerializer<E>::value>
struct ArraySerializationStrategy;

// Serialization strategy for standard mojom types. This branches further
// by choosing an ArraySerializer specialization from above.
template <typename E>
struct ArraySerializationStrategy<E, false> {
  static size_t GetSerializedSize(const Array<E>& input) {
    DCHECK(input);
    return ArraySerializer<E, typename WrapperTraits<E>::DataType>::
        GetSerializedSize(input);
  }

  template <typename F>
  static void Serialize(Array<E> input,
                        Buffer* buf,
                        Array_Data<F>** output,
                        const ArrayValidateParams* validate_params) {
    DCHECK(input);
    Array_Data<F>* result = Array_Data<F>::New(input.size(), buf);
    if (result) {
      ArraySerializer<E, F>::SerializeElements(std::move(input), buf, result,
                                               validate_params);
    }
    *output = result;
  }

  template <typename F>
  static bool Deserialize(Array_Data<F>* input,
                          Array<E>* output,
                          SerializationContext* context) {
    DCHECK(input);
    return ArraySerializer<E, F>::DeserializeElements(input, output, context);
  }
};

// Serialization for arrays of native-only types, which are opaquely serialized
// as arrays of uint8_t arrays.
template <typename E>
struct ArraySerializationStrategy<E, true> {
  static size_t GetSerializedSize(const Array<E>& input) {
    DCHECK(input);
    DCHECK_LE(input.size(), std::numeric_limits<uint32_t>::max());
    size_t size = ArrayDataTraits<Array_Data<uint8_t>*>::GetStorageSize(
        static_cast<uint32_t>(input.size()));
    for (size_t i = 0; i < input.size(); ++i) {
      size_t element_size = GetSerializedSizeNative_(input[i]);
      DCHECK_LT(element_size, std::numeric_limits<uint32_t>::max());
      size += ArrayDataTraits<uint8_t>::GetStorageSize(
          static_cast<uint32_t>(element_size));
    }
    return size;
  }

  template <typename F>
  static void Serialize(Array<E> input,
                        Buffer* buf,
                        Array_Data<F>** output,
                        const ArrayValidateParams* validate_params) {
    static_assert(
        std::is_same<F, Array_Data<uint8_t>*>::value,
        "Native-only type array must serialize to array of byte arrays.");
    DCHECK(input);
    DCHECK(validate_params);
    // TODO(rockot): We may want to support nullable (i.e. scoped_ptr<T>)
    // elements here.
    DCHECK(!validate_params->element_is_nullable);
    Array_Data<Array_Data<uint8_t>*>* result =
        Array_Data<Array_Data<uint8_t>*>::New(input.size(), buf);
    for (size_t i = 0; i < input.size(); ++i)
      SerializeNative_(input[i], buf, &result->at(i));
    *output = result;
  }

  template <typename F>
  static bool Deserialize(Array_Data<F>* input,
                          Array<E>* output,
                          SerializationContext* context) {
    static_assert(
        std::is_same<F, Array_Data<uint8_t>*>::value,
        "Native-only type array must deserialize from array of byte arrays.");
    DCHECK(input);

    Array<E> result(input->size());
    bool success = true;
    for (size_t i = 0; i < input->size(); ++i) {
      // We don't short-circuit on failure since we can't know what the native
      // type's ParamTraits' expectations are.
      success = success &&
          DeserializeNative_(input->at(i), &result[i], context);
    }
    output->Swap(&result);
    return success;
  }
};

}  // namespace internal

template <typename E>
inline size_t GetSerializedSize_(const Array<E>& input) {
  if (!input)
    return 0;
  using Strategy = internal::ArraySerializationStrategy<
      E, internal::ShouldUseNativeSerializer<E>::value>;
  return Strategy::GetSerializedSize(input);
}

template <typename E, typename F>
inline void SerializeArray_(
    Array<E> input,
    internal::Buffer* buf,
    internal::Array_Data<F>** output,
    const internal::ArrayValidateParams* validate_params) {
  using Strategy = internal::ArraySerializationStrategy<
      E, internal::ShouldUseNativeSerializer<E>::value>;
  if (input) {
    MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
        validate_params->expected_num_elements != 0 &&
            input.size() != validate_params->expected_num_elements,
        internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
        internal::MakeMessageWithExpectedArraySize(
            "fixed-size array has wrong number of elements", input.size(),
            validate_params->expected_num_elements));
    Strategy::template Serialize<F>(std::move(input), buf, output,
                                    validate_params);
  } else {
    *output = nullptr;
  }
}

template <typename E, typename F>
inline bool Deserialize_(internal::Array_Data<F>* input,
                         Array<E>* output,
                         internal::SerializationContext* context) {
  using Strategy = internal::ArraySerializationStrategy<
      E, internal::ShouldUseNativeSerializer<E>::value>;
  if (input)
    return Strategy::template Deserialize<F>(input, output, context);
  *output = nullptr;
  return true;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
