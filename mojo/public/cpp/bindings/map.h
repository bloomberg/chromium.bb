// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_H_

#include <map>

#include "mojo/public/cpp/bindings/lib/map_internal.h"

namespace mojo {

template <typename Key, typename Value>
class Map {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(Map, RValue)
 public:
  // Map keys can not be move only classes.
  static_assert(!internal::IsMoveOnlyType<Key>::value,
                "Map keys can not be move only types.");

  typedef internal::MapTraits<Key,
                              Value,
                              internal::IsMoveOnlyType<Value>::value> Traits;
  typedef typename Traits::KeyStorageType KeyStorageType;
  typedef typename Traits::KeyRefType KeyRefType;
  typedef typename Traits::KeyConstRefType KeyConstRefType;
  typedef typename Traits::KeyForwardType KeyForwardType;

  typedef typename Traits::ValueStorageType ValueStorageType;
  typedef typename Traits::ValueRefType ValueRefType;
  typedef typename Traits::ValueConstRefType ValueConstRefType;
  typedef typename Traits::ValueForwardType ValueForwardType;

  typedef internal::Map_Data<typename internal::WrapperTraits<Key>::DataType,
                             typename internal::WrapperTraits<Value>::DataType>
      Data_;

  Map() : is_null_(true) {}

  Map(mojo::Array<Key> keys, mojo::Array<Value> values) : is_null_(false) {
    MOJO_DCHECK(keys.size() == values.size());
    Traits::InitializeFrom(&map_, keys.Pass(), values.Pass());
  }

  ~Map() { Traits::Finalize(&map_); }

  Map(RValue other) : is_null_(true) { Take(other.object); }
  Map& operator=(RValue other) {
    Take(other.object);
    return *this;
  }

  template <typename U>
  static Map From(const U& other) {
    return TypeConverter<Map, U>::Convert(other);
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, Map>::Convert(*this);
  }

  void reset() {
    if (!map_.empty()) {
      Traits::Finalize(&map_);
      map_.clear();
    }
    is_null_ = true;
  }

  bool is_null() const { return is_null_; }

  size_t size() const { return map_.size(); }

  // Used to mark an empty map as non-null for serialization purposes.
  void mark_non_null() { is_null_ = false; }

  // Inserts a key-value pair into the map. Like std::map, this does not insert
  // |value| if |key| is already a member of the map.
  void insert(KeyForwardType key, ValueForwardType value) {
    is_null_ = false;
    Traits::Insert(&map_, key, value);
  }

  ValueRefType at(KeyForwardType key) { return Traits::at(&map_, key); }
  ValueConstRefType at(KeyForwardType key) const {
    return Traits::at(&map_, key);
  }

  void Swap(Map<Key, Value>* other) {
    std::swap(is_null_, other->is_null_);
    map_.swap(other->map_);
  }
  void Swap(std::map<Key, Value>* other) {
    is_null_ = false;
    map_.swap(*other);
  }

  // This moves all values in the map to a set of parallel arrays. This action
  // is destructive because we can have move-only objects as values; therefore
  // we can't have copy semantics here.
  void DecomposeMapTo(mojo::Array<Key>* keys, mojo::Array<Value>* values) {
    Traits::Decompose(&map_, keys, values);
    Traits::Finalize(&map_);
    map_.clear();
    is_null_ = true;
  }

  // Please note that calling this method will fail compilation if the value
  // type cannot be cloned (which usually means that it is a Mojo handle type or
  // a type contains Mojo handles).
  Map Clone() const {
    Map result;
    result.is_null_ = is_null_;
    Traits::Clone(map_, &result.map_);
    return result.Pass();
  }

  bool Equals(const Map& other) const {
    if (is_null() != other.is_null())
      return false;
    if (size() != other.size())
      return false;
    auto i = begin();
    auto j = other.begin();
    while (i != end()) {
      if (i.GetKey() != j.GetKey())
        return false;
      if (!internal::ValueTraits<Value>::Equals(i.GetValue(), j.GetValue()))
        return false;
      ++i;
      ++j;
    }
    return true;
  }

  class ConstMapIterator {
   public:
    ConstMapIterator(
        const typename std::map<KeyStorageType,
                                ValueStorageType>::const_iterator& it)
        : it_(it) {}

    KeyConstRefType GetKey() { return Traits::GetKey(it_); }
    ValueConstRefType GetValue() { return Traits::GetValue(it_); }

    ConstMapIterator& operator++() {
      it_++;
      return *this;
    }
    bool operator!=(const ConstMapIterator& rhs) const {
      return it_ != rhs.it_;
    }
    bool operator==(const ConstMapIterator& rhs) const {
      return it_ == rhs.it_;
    }

   private:
    typename std::map<KeyStorageType, ValueStorageType>::const_iterator it_;
  };

  // Provide read-only iteration over map members.
  ConstMapIterator begin() const { return ConstMapIterator(map_.begin()); }
  ConstMapIterator end() const { return ConstMapIterator(map_.end()); }

  ConstMapIterator find(KeyForwardType key) const {
    return ConstMapIterator(map_.find(key));
  }

 private:
  typedef std::map<KeyStorageType, ValueStorageType> Map::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &Map::map_; }

 private:
  void Take(Map* other) {
    reset();
    Swap(other);
  }

  std::map<KeyStorageType, ValueStorageType> map_;
  bool is_null_;
};

template <typename MojoKey,
          typename MojoValue,
          typename STLKey,
          typename STLValue>
struct TypeConverter<Map<MojoKey, MojoValue>, std::map<STLKey, STLValue>> {
  static Map<MojoKey, MojoValue> Convert(
      const std::map<STLKey, STLValue>& input) {
    Map<MojoKey, MojoValue> result;
    result.mark_non_null();
    for (auto& pair : input) {
      result.insert(TypeConverter<MojoKey, STLKey>::Convert(pair.first),
                    TypeConverter<MojoValue, STLValue>::Convert(pair.second));
    }
    return result.Pass();
  }
};

template <typename MojoKey,
          typename MojoValue,
          typename STLKey,
          typename STLValue>
struct TypeConverter<std::map<STLKey, STLValue>, Map<MojoKey, MojoValue>> {
  static std::map<STLKey, STLValue> Convert(
      const Map<MojoKey, MojoValue>& input) {
    std::map<STLKey, STLValue> result;
    if (!input.is_null()) {
      for (auto it = input.begin(); it != input.end(); ++it) {
        result.insert(std::make_pair(
            TypeConverter<STLKey, MojoKey>::Convert(it.GetKey()),
            TypeConverter<STLValue, MojoValue>::Convert(it.GetValue())));
      }
    }
    return result;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_H_
