// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SYNCHRONIZED_MAP_H_
#define CHROME_TEST_CHROMEDRIVER_SYNCHRONIZED_MAP_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

template <typename K, typename V>
class SynchronizedMap {
 public:
  SynchronizedMap();
  ~SynchronizedMap();

  void Set(const K& key, const V& value);
  bool Get(const K& key, V* value) const;
  bool Has(const K& key) const;
  bool Remove(const K& key);

  void GetKeys(std::vector<K>* keys) const;

 private:
  typedef std::map<K, V> Map;
  Map map_;
  mutable base::Lock map_lock_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizedMap);
};

template <typename K, typename V>
SynchronizedMap<K, V>::SynchronizedMap() {}

template <typename K, typename V>
SynchronizedMap<K, V>::~SynchronizedMap() {}

template <typename K, typename V>
void SynchronizedMap<K, V>::Set(const K& key, const V& value) {
  base::AutoLock lock(map_lock_);
  typename Map::iterator iter = map_.find(key);
  if (iter != map_.end())
    map_.erase(iter);
  map_.insert(std::make_pair(key, value));
}

template <typename K, typename V>
bool SynchronizedMap<K, V>::Get(const K& key, V* value) const {
  base::AutoLock lock(map_lock_);
  typename Map::const_iterator iter = map_.find(key);
  if (iter == map_.end())
    return false;
  *value = iter->second;
  return true;
}

template <typename K, typename V>
bool SynchronizedMap<K, V>::Has(const K& key) const {
  base::AutoLock lock(map_lock_);
  return map_.find(key) != map_.end();
}

template <typename K, typename V>
bool SynchronizedMap<K, V>::Remove(const K& key) {
  base::AutoLock lock(map_lock_);
  typename Map::iterator iter = map_.find(key);
  if (iter == map_.end())
    return false;
  map_.erase(iter);
  return true;
}

template <typename K, typename V>
void SynchronizedMap<K, V>::GetKeys(std::vector<K>* keys) const {
  keys->clear();
  base::AutoLock lock(map_lock_);
  typename Map::const_iterator iter;
  for (iter = map_.begin(); iter != map_.end(); iter++)
    keys->push_back(iter->first);
}

#endif  // CHROME_TEST_CHROMEDRIVER_SYNCHRONIZED_MAP_H_
