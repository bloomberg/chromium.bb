// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_DATA_VIEW_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/map.h"

namespace mojo {

template <typename K, typename V>
class MapDataView {
 public:
  using Data_ = typename internal::MojomTypeTraits<
      Map<typename internal::DataViewTraits<K>::MojomType,
          typename internal::DataViewTraits<V>::MojomType>>::Data;

  MapDataView() {}

  MapDataView(Data_* data, internal::SerializationContext* context)
      : keys_(data ? data->keys.Get() : nullptr, context),
        values_(data ? data->values.Get() : nullptr, context) {}

  bool is_null() const {
    DCHECK_EQ(keys_.is_null(), values_.is_null());
    return keys_.is_null();
  }

  size_t size() const {
    DCHECK_EQ(keys_.size(), values_.size());
    return keys_.size();
  }

  ArrayDataView<K>& keys() { return keys_; }
  const ArrayDataView<K>& keys() const { return keys_; }

  template <typename U>
  bool ReadKeys(U* output) {
    return internal::Deserialize<
        Array<typename internal::DataViewTraits<K>::MojomType>>(
        keys_.data_, output, keys_.context_);
  }

  ArrayDataView<V>& values() { return values_; }
  const ArrayDataView<V>& values() const { return values_; }

  template <typename U>
  bool ReadValues(U* output) {
    return internal::Deserialize<
        Array<typename internal::DataViewTraits<V>::MojomType>>(
        values_.data_, output, values_.context_);
  }

 private:
  ArrayDataView<K> keys_;
  ArrayDataView<V> values_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_DATA_VIEW_H_
