// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCOPED_OBSERVER_WITH_DUPLICATED_SOURCES_H_
#define ASH_SHELF_SCOPED_OBSERVER_WITH_DUPLICATED_SOURCES_H_

#include <map>

#include "base/basictypes.h"
#include "base/logging.h"

// ScopedObserverWithDuplicatedSources is used to keep track of the set of
// sources an object has attached itself to as an observer. When
// ScopedObserverWithDuplicatedSources is destroyed it removes the object as an
// observer from all sources it has been added to.
// ScopedObserverWithDuplicatedSources adds |observer| once for a particular
// source, no matter how many times Add() is invoked. Additionaly |observer| is
// only removed once Remove() is invoked the same number of times as Add().

template <class Source, class Observer>
class ScopedObserverWithDuplicatedSources {
 public:
  explicit ScopedObserverWithDuplicatedSources(Observer* observer)
      : observer_(observer) {}

  ~ScopedObserverWithDuplicatedSources() {
    typename SourceToAddCountMap::const_iterator iter =
        counted_sources_.begin();
    for (; iter != counted_sources_.end(); ++iter)
      iter->first->RemoveObserver(observer_);
  }

  // Adds the object passed to the constructor only once as an observer on
  // |source|.
  void Add(Source* source) {
    if (counted_sources_.find(source) == counted_sources_.end())
      source->AddObserver(observer_);
    counted_sources_[source]++;
  }

  // Only remove the object passed to the constructor as an observer from
  // |source| when Remove() is invoked the same number of times as Add().
  void Remove(Source* source) {
    typename SourceToAddCountMap::iterator iter =
        counted_sources_.find(source);
    DCHECK(iter != counted_sources_.end() && iter->second > 0);

    if (--iter->second == 0) {
      counted_sources_.erase(source);
      source->RemoveObserver(observer_);
    }
  }

  bool IsObserving(Source* source) const {
    return counted_sources_.find(source) != counted_sources_.end();
  }

 private:
  typedef std::map<Source*, int> SourceToAddCountMap;

  Observer* observer_;

  // Map Source and its adding count.
  std::map<Source*, int> counted_sources_;

  DISALLOW_COPY_AND_ASSIGN(ScopedObserverWithDuplicatedSources);
};

#endif  // ASH_SHELF_SCOPED_OBSERVER_WITH_DUPLICATED_SOURCES_H_
