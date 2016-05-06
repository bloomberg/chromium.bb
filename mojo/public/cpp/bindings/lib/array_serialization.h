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
  // String, array, map and struct.
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
          typename MaybeConstUserType,
          ArraySerializerType type =
              GetArraySerializerType<typename MojomType::Data_::Element>::value>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename MojomType, typename MaybeConstUserType>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       ArraySerializerType::POD> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");
  static_assert(std::is_same<Element, typename Traits::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(MaybeConstUserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align(Traits::GetSize(input) * sizeof(DataElement));
  }

  static void SerializeElements(MaybeConstUserType& input,
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
template <typename MojomType, typename MaybeConstUserType>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       ArraySerializerType::BOOLEAN> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomType::Data_;

  static_assert(std::is_same<bool, typename UserType::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(MaybeConstUserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align((Traits::GetSize(input) + 7) / 8);
  }

  static void SerializeElements(MaybeConstUserType& input,
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
template <typename MojomType, typename MaybeConstUserType>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       ArraySerializerType::HANDLE> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(std::is_same<Element, typename Traits::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(MaybeConstUserType& input,
                                  SerializationContext* context) {
    return sizeof(Data) +
           Align(Traits::GetSize(input) * sizeof(typename Data::Element));
  }

  static void SerializeElements(MaybeConstUserType& input,
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
      Traits::GetAt(*output, i) = MakeScopedHandle(
          HandleType(context->handles.TakeHandle(input->at(i)).value()));
    }
    return true;
  }
};

// This template must only apply to pointer mojo entity (strings, structs,
// arrays and maps).
template <typename MojomType, typename MaybeConstUserType>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       ArraySerializerType::POINTER> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static size_t GetSerializedSize(MaybeConstUserType& input,
                                  SerializationContext* context) {
    size_t element_count = Traits::GetSize(input);
    size_t size =
        sizeof(Data) +
        element_count *
            sizeof(Pointer<typename std::remove_pointer<DataElement>::type>);
    for (size_t i = 0; i < element_count; ++i)
      size += PrepareToSerialize<Element>(Traits::GetAt(input, i), context);
    return size;
  }

  static void SerializeElements(MaybeConstUserType& input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i) {
      DataElement element;
      SerializeCaller<Element>::Run(Traits::GetAt(input, i), buf, &element,
                                    validate_params->element_validate_params,
                                    context);
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
      if (!Deserialize<Element>(input->at(i), &Traits::GetAt(*output, i),
                                context)) {
        success = false;
      }
    }
    return success;
  }

 private:
  template <typename T,
            bool is_array_or_map = IsSpecializationOf<Array, T>::value ||
                                   IsSpecializationOf<Map, T>::value>
  struct SerializeCaller {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    Buffer* buf,
                    DataElement* output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      Serialize<T>(std::forward<InputElementType>(input), buf, output, context);
    }
  };

  template <typename T>
  struct SerializeCaller<T, true> {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    Buffer* buf,
                    DataElement* output,
                    const ArrayValidateParams* validate_params,
                    SerializationContext* context) {
      Serialize<T>(std::forward<InputElementType>(input), buf, output,
                   validate_params, context);
    }
  };
};

// Handles serialization and deserialization of arrays of unions.
template <typename MojomType, typename MaybeConstUserType>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       ArraySerializerType::UNION> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using Traits = ArrayTraits<UserType>;

  static_assert(std::is_same<typename MojomType::Element,
                             typename Traits::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(MaybeConstUserType& input,
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

  static void SerializeElements(MaybeConstUserType& input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = Traits::GetSize(input);
    for (size_t i = 0; i < size; ++i) {
      typename Data::Element* result = output->storage() + i;
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

template <typename Element, typename MaybeConstUserType>
struct Serializer<Array<Element>, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Impl = ArraySerializer<Array<Element>, MaybeConstUserType>;
  using Traits = ArrayTraits<UserType>;
  using Data = typename Array<Element>::Data_;

  static size_t PrepareToSerialize(MaybeConstUserType& input,
                                   SerializationContext* context) {
    if (CallIsNullIfExists<Traits>(input))
      return 0;
    return Impl::GetSerializedSize(input, context);
  }

  static void Serialize(MaybeConstUserType& input,
                        Buffer* buf,
                        Data** output,
                        const ArrayValidateParams* validate_params,
                        SerializationContext* context) {
    if (!CallIsNullIfExists<Traits>(input)) {
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          validate_params->expected_num_elements != 0 &&
              Traits::GetSize(input) != validate_params->expected_num_elements,
          internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
          internal::MakeMessageWithExpectedArraySize(
              "fixed-size array has wrong number of elements",
              Traits::GetSize(input), validate_params->expected_num_elements));
      Data* result = Data::New(Traits::GetSize(input), buf);
      if (result)
        Impl::SerializeElements(input, buf, result, validate_params, context);
      *output = result;
    } else {
      *output = nullptr;
    }
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    if (input)
      return Impl::DeserializeElements(input, output, context);
    Traits::SetToNull(*output);
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
