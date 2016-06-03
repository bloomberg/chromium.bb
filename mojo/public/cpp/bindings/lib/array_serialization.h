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
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/map.h"

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

// Used as the UserTypeReader template parameter of ArraySerializer.
template <typename MaybeConstUserType>
class ArrayReader {
 public:
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;

  explicit ArrayReader(MaybeConstUserType& input) : input_(input) {}
  ~ArrayReader() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  using GetNextResult =
      decltype(Traits::GetAt(std::declval<MaybeConstUserType&>(), 0));
  GetNextResult GetNext() {
    DCHECK_LT(index_, Traits::GetSize(input_));
    return Traits::GetAt(input_, index_++);
  }

  using GetDataIfExistsResult = decltype(
      CallGetDataIfExists<Traits>(std::declval<MaybeConstUserType&>()));
  GetDataIfExistsResult GetDataIfExists() {
    return CallGetDataIfExists<Traits>(input_);
  }

 private:
  MaybeConstUserType& input_;
  size_t index_ = 0;
};

// ArraySerializer is also used to serialize map keys and values. Therefore, it
// has a UserTypeReader parameter which is an adaptor for reading to hide the
// difference between ArrayTraits and MapTraits.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader,
          ArraySerializerType type =
              GetArraySerializerType<typename MojomType::Data_::Element>::value>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeReader,
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

  static size_t GetSerializedSize(UserTypeReader* input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align(input->GetSize() * sizeof(DataElement));
  }

  static void SerializeElements(UserTypeReader* input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    size_t size = input->GetSize();
    if (size == 0)
      return;

    auto data = input->GetDataIfExists();
    if (data) {
      memcpy(output->storage(), data, size * sizeof(DataElement));
    } else {
      for (size_t i = 0; i < size; ++i)
        output->at(i) = static_cast<DataElement>(input->GetNext());
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    Traits::Resize(*output, input->size());
    if (input->size()) {
      auto data = CallGetDataIfExists<Traits>(*output);
      if (data) {
        memcpy(data, input->storage(), input->size() * sizeof(DataElement));
      } else {
        for (size_t i = 0; i < input->size(); ++i)
          Traits::GetAt(*output, i) = static_cast<Element>(input->at(i));
      }
    }
    return true;
  }
};

// Serializes and deserializes arrays of bools.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeReader,
                       ArraySerializerType::BOOLEAN> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomType::Data_;

  static_assert(std::is_same<bool, typename UserType::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(UserTypeReader* input,
                                  SerializationContext* context) {
    return sizeof(Data) + Align((input->GetSize() + 7) / 8);
  }

  static void SerializeElements(UserTypeReader* input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i)
      output->at(i) = input->GetNext();
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    Traits::Resize(*output, input->size());
    for (size_t i = 0; i < input->size(); ++i)
      Traits::GetAt(*output, i) = input->at(i);
    return true;
  }
};

// Serializes and deserializes arrays of handles.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeReader,
                       ArraySerializerType::HANDLE> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(std::is_same<Element, typename Traits::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(UserTypeReader* input,
                                  SerializationContext* context) {
    return sizeof(Data) +
           Align(input->GetSize() * sizeof(typename Data::Element));
  }

  static void SerializeElements(UserTypeReader* input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle type should not have array validate params";

    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      // Transfer ownership of the handle.
      output->at(i) = context->handles.AddHandle(input->GetNext().release());
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
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeReader,
                       ArraySerializerType::POINTER> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static size_t GetSerializedSize(UserTypeReader* input,
                                  SerializationContext* context) {
    size_t element_count = input->GetSize();
    size_t size =
        sizeof(Data) +
        element_count *
            sizeof(Pointer<typename std::remove_pointer<DataElement>::type>);
    for (size_t i = 0; i < element_count; ++i)
      size += PrepareToSerialize<Element>(input->GetNext(), context);
    return size;
  }

  static void SerializeElements(UserTypeReader* input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      DataElement element;
      SerializeCaller<Element>::Run(input->GetNext(), buf, &element,
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
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeReader>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeReader,
                       ArraySerializerType::UNION> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomType::Data_;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(std::is_same<typename MojomType::Element,
                             typename Traits::Element>::value,
                "Incorrect array serializer");

  static size_t GetSerializedSize(UserTypeReader* input,
                                  SerializationContext* context) {
    size_t element_count = input->GetSize();
    size_t size = sizeof(Data);
    for (size_t i = 0; i < element_count; ++i) {
      // Call with |inlined| set to false, so that it will account for both the
      // data in the union and the space in the array used to hold the union.
      size += PrepareToSerialize<Element>(input->GetNext(), false, context);
    }
    return size;
  }

  static void SerializeElements(UserTypeReader* input,
                                Buffer* buf,
                                Data* output,
                                const ArrayValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      typename Data::Element* result = output->storage() + i;
      Serialize<Element>(input->GetNext(), buf, &result, true, context);
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
      if (!Deserialize<Element>(&input->at(i), &Traits::GetAt(*output, i),
                                context)) {
        success = false;
      }
    }
    return success;
  }
};

template <typename Element, typename MaybeConstUserType>
struct Serializer<Array<Element>, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Impl = ArraySerializer<Array<Element>,
                               MaybeConstUserType,
                               ArrayReader<MaybeConstUserType>>;
  using Traits = ArrayTraits<UserType>;
  using Data = typename Array<Element>::Data_;

  static size_t PrepareToSerialize(MaybeConstUserType& input,
                                   SerializationContext* context) {
    if (CallIsNullIfExists<Traits>(input))
      return 0;
    ArrayReader<MaybeConstUserType> reader(input);
    return Impl::GetSerializedSize(&reader, context);
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
      if (result) {
        ArrayReader<MaybeConstUserType> reader(input);
        Impl::SerializeElements(&reader, buf, result, validate_params, context);
      }
      *output = result;
    } else {
      *output = nullptr;
    }
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    return Impl::DeserializeElements(input, output, context);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
