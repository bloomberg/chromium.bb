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

enum class ArraySerializerType {
  BOOLEAN,
  // Except boolean.
  POD,
  HANDLE,
  POINTER,
  UNION
};

template <typename T>
struct GetArraySerializerType {
  static const ArraySerializerType value =
      IsUnionDataType<T>::value
          ? ArraySerializerType::UNION
          : (std::is_pointer<T>::value
                 ? ArraySerializerType::POINTER
                 : (IsHandle<T>::value ? ArraySerializerType::HANDLE
                                       : (std::is_same<T, bool>::value
                                              ? ArraySerializerType::BOOLEAN
                                              : ArraySerializerType::POD)));
};

template <typename MojomType,
          typename UserType,
          ArraySerializerType type =
              GetArraySerializerType<typename MojomType::Data_::Element>::value>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename MojomType, typename UserType>
struct ArraySerializer<MojomType, UserType, ArraySerializerType::POD> {
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;
  using UserElement = typename Traits::Element;

  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");
  static_assert(std::is_same<Element, UserElement>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align(Traits::GetSize(input) * sizeof(DataElement));
  }

  static void SerializeElements(UserType input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    if (Traits::GetSize(input)) {
      memcpy(output->storage(), Traits::GetData(input),
             Traits::GetSize(input) * sizeof(DataElement));
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    Traits::Resize(*output, input->size());
    if (input->size()) {
      memcpy(Traits::GetData(*output), input->storage(),
             input->size() * sizeof(DataElement));
    }
    return true;
  }
};

// Serializes and deserializes arrays of bools.
template <typename MojomType, typename UserType>
struct ArraySerializer<MojomType, UserType, ArraySerializerType::BOOLEAN> {
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomType::Data_;

  static_assert(std::is_same<bool, typename UserType::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align((Traits::GetSize(input) + 7) / 8);
  }

  static void SerializeElements(UserType input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i)
      output->at(i) = Traits::GetAt(input, i);
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    Traits::Resize(*output, input->size());
    // TODO(darin): Can this be a memcpy somehow instead of a bit-by-bit copy?
    for (size_t i = 0; i < input->size(); ++i)
      Traits::GetAt(*output, i) = input->at(i);
    return true;
  }
};

// Serializes and deserializes arrays of handles.
template <typename MojomType, typename UserType>
struct ArraySerializer<MojomType, UserType, ArraySerializerType::HANDLE> {
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;
  using UserElement = typename Traits::Element;

  static_assert(std::is_same<Element, UserElement>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align(Traits::GetSize(input) * sizeof(DataElement));
  }

  static void SerializeElements(UserType input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle type should not have array validate params";

    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i) {
      // Transfer ownership of the handle.
      output->at(i) =
          context->handles.AddHandle(Traits::GetAt(input, i).release());
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !output->at(i).is_valid(),
          VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
          MakeMessageWithArrayIndex(
              "invalid handle in array expecting valid handles", size, i));
    }
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    using HandleType = typename Element::RawHandleType;
    Traits::Resize(*output, input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      Traits::GetAt(*output, i) = MakeScopedHandle(HandleType(
          context->handles.TakeHandle(input->at(i)).value()));
    }
    return true;
  }
};

// This template must only apply to pointer mojo entity (strings, structs,
// arrays and maps).
template <typename MojomType, typename UserType>
struct ArraySerializer<MojomType, UserType, ArraySerializerType::POINTER> {
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;
  using UserElement = typename Traits::Element;

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    size_t element_count = Traits::GetSize(input);
    size_t size =
        sizeof(Data) +
        element_count *
            sizeof(Pointer<typename std::remove_pointer<DataElement>::type>);
    for (size_t i = 0; i < element_count; ++i)
      size += GetSerializedSize_(Traits::GetAt(input, i), context);
    return size;
  }

  static void SerializeElements(UserType input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i) {
      DataElement element;
      SerializeCaller<Element>::Run(
          std::move(Traits::GetAt(input, i)), buf, &element,
          validate_params->element_validate_params, context);
      output->at(i) = element;
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && !element,
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    size, i));
    }
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    bool success = true;
    Traits::Resize(*output, input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      // Note that we rely on complete deserialization taking place in order to
      // transfer ownership of all encoded handles. Therefore we don't
      // short-circuit on failure here.
      if (!Deserialize_(input->at(i), &Traits::GetAt(*output, i), context))
        success = false;
    }
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
                    DataElement* output,
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
                    DataElement* output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      SerializeArray_(std::move(input), buf, output, validate_params, context);
    }
  };

  template <typename T, typename U>
  struct SerializeCaller<Map<T, U>, false, false> {
    static void Run(Map<T, U> input,
                    Buffer* buf,
                    DataElement* output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      SerializeMap_(std::move(input), buf, output, validate_params, context);
    }
  };

  template <typename T>
  struct SerializeCaller<T, false, true> {
    static void Run(const T& input,
                    Buffer* buf,
                    DataElement* output,
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
template <typename MojomType, typename UserType>
struct ArraySerializer<MojomType, UserType, ArraySerializerType::UNION> {
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;
  using UserElement = typename Traits::Element;

  static_assert(std::is_same<Element, UserElement>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    size_t element_count = Traits::GetSize(input);
    size_t size = sizeof(Data);
    for (size_t i = 0; i < element_count; ++i) {
      // Call GetSerializedSize_ with |inlined| set to false, so that it will
      // account for both the data in the union and the space in the array used
      // to hold the union.
      size += GetSerializedSize_(Traits::GetAt(input, i), false, context);
    }
    return size;
  }

  static void SerializeElements(UserType input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i) {
      DataElement* result = output->storage() + i;
      SerializeUnion_(std::move(Traits::GetAt(input, i)), buf, &result, true,
                      context);
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && output->at(i).is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    size, i));
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    bool success = true;
    Traits::Resize(*output, input->size());
    for (size_t i = 0; i < input->size(); ++i) {
      // Note that we rely on complete deserialization taking place in order to
      // transfer ownership of all encoded handles. Therefore we don't
      // short-circuit on failure here.
      if (!Deserialize_(&input->at(i), &Traits::GetAt(*output, i), context))
        success = false;
    }
    return success;
  }
};

// Another layer of abstraction to switch between standard mojom type
// serializers and native-only serializers.
template <typename MojomType,
          typename UserType,
          bool use_native =
              ShouldUseNativeSerializer<typename MojomType::Element>::value>
struct ArraySerializationStrategy;

// Serialization strategy for standard mojom types. This branches further
// by choosing an ArraySerializer specialization from above.
template <typename MojomType, typename UserType>
struct ArraySerializationStrategy<MojomType, UserType, false> {
  using Serializer = ArraySerializer<MojomType, UserType>;
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomType::Data_;

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    DCHECK(!Traits::IsNull(input));
    return Serializer::GetSerializedSize(input, context);
  }

  static void Serialize(UserType input,
                        Buffer* buf,
                        Data** output,
                        const ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    DCHECK(!Traits::IsNull(input));
    Data* result = Data::New(Traits::GetSize(input), buf);
    if (result) {
      Serializer::SerializeElements(std::move(input), buf, result,
                                    validate_params, context);
    }
    *output = result;
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    DCHECK(input);
    return Serializer::DeserializeElements(input, output, context);
  }
};

// Serialization for arrays of native-only types, which are opaquely serialized
// as arrays of uint8_t arrays.
template <typename MojomType, typename UserType>
struct ArraySerializationStrategy<MojomType, UserType, true> {
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomType::Data_;

  static_assert(
      std::is_same<Data, Array_Data<NativeStruct_Data*>>::value,
      "Mismatched types for serializing array of native-only structs.");

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    DCHECK(!Traits::IsNull(input));

    size_t element_count = Traits::GetSize(input);
    DCHECK_LE(element_count, std::numeric_limits<uint32_t>::max());
    size_t size = ArrayDataTraits<NativeStruct_Data*>::GetStorageSize(
        static_cast<uint32_t>(element_count));
    for (size_t i = 0; i < element_count; ++i) {
      size_t element_size =
          GetSerializedSizeNative_(Traits::GetAt(input, i), context);
      DCHECK_LT(element_size, std::numeric_limits<uint32_t>::max());
      size += static_cast<uint32_t>(element_size);
    }
    return size;
  }

  static void Serialize(UserType input,
                        Buffer* buf,
                        Data** output,
                        const ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    DCHECK(!Traits::IsNull(input));
    DCHECK(validate_params);
    // TODO(rockot): We may want to support nullable (i.e. scoped_ptr<T>)
    // elements here.
    DCHECK(!validate_params->element_is_nullable);
    size_t element_count = Traits::GetSize(input);
    Data* result = Data::New(element_count, buf);
    for (size_t i = 0; i < element_count; ++i)
      SerializeNative_(Traits::GetAt(input, i), buf, &result->at(i), context);
    *output = result;
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    DCHECK(input);

    Traits::Resize(*output, input->size());
    bool success = true;
    for (size_t i = 0; i < input->size(); ++i) {
      // We don't short-circuit on failure since we can't know what the native
      // type's ParamTraits' expectations are.
      if (!DeserializeNative_(input->at(i), &Traits::GetAt(*output, i),
                              context))
        success = false;
    }
    return success;
  }
};

template <typename MojomType, typename UserType>
struct ArraySerializationImpl {
  using Traits = ArrayTraits<UserType>;
  using Strategy = ArraySerializationStrategy<MojomType, UserType>;

  static size_t GetSerializedSize(const UserType& input,
                                  SerializationContext* context) {
    if (Traits::IsNull(input))
      return 0;
    return Strategy::GetSerializedSize(input, context);
  }

  static void Serialize(UserType input,
                        internal::Buffer* buf,
                        typename MojomType::Data_** output,
                        const internal::ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    if (!Traits::IsNull(input)) {
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          validate_params->expected_num_elements != 0 &&
              Traits::GetSize(input) != validate_params->expected_num_elements,
          internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
          internal::MakeMessageWithExpectedArraySize(
              "fixed-size array has wrong number of elements",
              Traits::GetSize(input), validate_params->expected_num_elements));
      Strategy::Serialize(std::move(input), buf, output, validate_params,
                          context);
    } else {
      *output = nullptr;
    }
  }

  static bool Deserialize(typename MojomType::Data_* input,
                          UserType* output,
                          internal::SerializationContext* context) {
    if (input)
      return Strategy::Deserialize(input, output, context);
    Traits::SetToNull(*output);
    return true;
  }
};

}  // namespace internal

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_TRAITS_H_
