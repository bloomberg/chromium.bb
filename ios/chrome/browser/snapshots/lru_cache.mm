// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/lru_cache.h"

#include <stddef.h>

#include <memory>

#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"

namespace {

struct NSObjectEqualTo {
  bool operator()(const base::scoped_nsprotocol<id<NSObject>>& obj1,
                  const base::scoped_nsprotocol<id<NSObject>>& obj2) const {
    return [obj1 isEqual:obj2];
  }
};

struct NSObjectHash {
  std::size_t operator()(
      const base::scoped_nsprotocol<id<NSObject>>& obj) const {
    return [obj hash];
  }
};

template <class KeyType, class ValueType, class HashType>
struct MRUCacheNSObjectHashMap {
  typedef base::hash_map<KeyType, ValueType, HashType, NSObjectEqualTo> Type;
};

class NSObjectMRUCache
    : public base::MRUCacheBase<base::scoped_nsprotocol<id<NSObject>>,
                                base::scoped_nsprotocol<id<NSObject>>,
                                NSObjectHash,
                                MRUCacheNSObjectHashMap> {
 private:
  typedef base::MRUCacheBase<base::scoped_nsprotocol<id<NSObject>>,
                             base::scoped_nsprotocol<id<NSObject>>,
                             NSObjectHash,
                             MRUCacheNSObjectHashMap>
      ParentType;

 public:
  explicit NSObjectMRUCache(typename ParentType::size_type max_size)
      : ParentType(max_size) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NSObjectMRUCache);
};

}  // namespace

@implementation LRUCache {
  std::unique_ptr<NSObjectMRUCache> _cache;
}

@synthesize maxCacheSize = _maxCacheSize;

- (instancetype)init {
  NOTREACHED();  // Use initWithCacheSize: instead.
  return nil;
}

- (instancetype)initWithCacheSize:(NSUInteger)maxCacheSize {
  self = [super init];
  if (self) {
    _maxCacheSize = maxCacheSize;
    _cache.reset(new NSObjectMRUCache(self.maxCacheSize));
  }
  return self;
}

- (id)objectForKey:(id<NSObject>)key {
  base::scoped_nsprotocol<id<NSObject>> keyObj([key retain]);
  auto it = _cache->Get(keyObj);
  if (it == _cache->end())
    return nil;
  return it->second.get();
}

- (void)setObject:(id<NSObject>)value forKey:(NSObject*)key {
  base::scoped_nsprotocol<id<NSObject>> keyObj([key copy]);
  base::scoped_nsprotocol<id<NSObject>> valueObj([value retain]);
  _cache->Put(keyObj, valueObj);
}

- (void)removeObjectForKey:(id<NSObject>)key {
  base::scoped_nsprotocol<id<NSObject>> keyObj([key retain]);
  auto it = _cache->Peek(keyObj);
  if (it != _cache->end())
    _cache->Erase(it);
}

- (void)removeAllObjects {
  _cache->Clear();
}

- (NSUInteger)count {
  return _cache->size();
}

- (BOOL)isEmpty {
  return _cache->empty();
}

@end
