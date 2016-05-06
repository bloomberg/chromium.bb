// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/map_data_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/map.h"

namespace mojo {
namespace internal {

template <typename Key, typename Value>
struct MapContext {
  explicit MapContext(bool in_is_null) : is_null(in_is_null) {}

  bool is_null;
  Array<Key> keys;
  Array<Value> values;
};

template <typename Key, typename Value, typename MaybeConstUserType>
struct Serializer<Map<Key, Value>, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using UserKey = typename UserType::Key;
  using UserValue = typename UserType::Value;
  using Data = typename Map<Key, Value>::Data_;

  static_assert(std::is_same<MaybeConstUserType, UserType>::value,
                "Only support serialization of non-const Maps.");
  static_assert(IsSpecializationOf<Map, UserType>::value,
                "Custom mapping of mojom map is not supported yet.");

  static size_t PrepareToSerialize(UserType& input,
                                   SerializationContext* context) {
    auto map_context = new MapContext<UserKey, UserValue>(input.is_null());
    if (!context->custom_contexts)
      context->custom_contexts.reset(new std::queue<void*>());
    context->custom_contexts->push(map_context);

    if (!input)
      return 0;

    input.DecomposeMapTo(&map_context->keys, &map_context->values);

    size_t struct_overhead = sizeof(Data);
    size_t keys_size =
        internal::PrepareToSerialize<Array<Key>>(map_context->keys, context);
    size_t values_size = internal::PrepareToSerialize<Array<Value>>(
        map_context->values, context);

    return struct_overhead + keys_size + values_size;
  }

  // We don't need an ArrayValidateParams instance for key validation since
  // we can deduce it from the Key type. (which can only be primitive types or
  // non-nullable strings.)
  static void Serialize(UserType& input,
                        Buffer* buf,
                        Data** output,
                        const ArrayValidateParams* value_validate_params,
                        SerializationContext* context) {
    std::unique_ptr<MapContext<UserKey, UserValue>> map_context(
        static_cast<MapContext<UserKey, UserValue>*>(
            context->custom_contexts->front()));
    context->custom_contexts->pop();

    if (map_context->is_null) {
      *output = nullptr;
      return;
    }

    auto result = Data::New(buf);
    if (result) {
      const ArrayValidateParams* key_validate_params =
          MapKeyValidateParamsFactory<
              typename GetDataTypeAsArrayElement<Key>::Data>::Get();
      internal::Serialize<Array<Key>>(map_context->keys, buf, &result->keys.ptr,
                                      key_validate_params, context);
      internal::Serialize<Array<Value>>(map_context->values, buf,
                                        &result->values.ptr,
                                        value_validate_params, context);
    }
    *output = result;
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    bool success = true;
    if (input) {
      Array<UserKey> keys;
      Array<UserValue> values;

      // Note that we rely on complete deserialization taking place in order to
      // transfer ownership of all encoded handles. Therefore we don't
      // short-circuit on failure here.
      if (!internal::Deserialize<Array<Key>>(input->keys.ptr, &keys, context))
        success = false;
      if (!internal::Deserialize<Array<Value>>(input->values.ptr, &values,
                                               context)) {
        success = false;
      }

      *output = UserType(std::move(keys), std::move(values));
    } else {
      *output = nullptr;
    }
    return success;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_SERIALIZATION_H_
