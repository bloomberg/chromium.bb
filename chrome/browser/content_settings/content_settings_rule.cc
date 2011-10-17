// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/content_settings/content_settings_rule.h"

namespace content_settings {

Rule::Rule() {}

Rule::Rule(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value* value)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      value(value) {
  DCHECK(value);
}

Rule::~Rule() {}

RuleIterator::~RuleIterator() {}

EmptyRuleIterator::~EmptyRuleIterator() {}

bool EmptyRuleIterator::HasNext() const {
  return false;
}

Rule EmptyRuleIterator::Next() {
  NOTREACHED();
  return Rule();
}

ConcatenationIterator::ConcatenationIterator(
    ScopedVector<RuleIterator>* iterators,
    base::AutoLock* auto_lock)
    : auto_lock_(auto_lock) {
  iterators_.swap(*iterators);

  ScopedVector<RuleIterator>::iterator it = iterators_.begin();
  while (it != iterators_.end()) {
    if (!(*it)->HasNext())
      it = iterators_.erase(it);
    else
      ++it;
  }
}

ConcatenationIterator::~ConcatenationIterator() {}

bool ConcatenationIterator::HasNext() const {
  return (!iterators_.empty());
}

Rule ConcatenationIterator::Next() {
  ScopedVector<RuleIterator>::iterator current_iterator =
      iterators_.begin();
  DCHECK(current_iterator != iterators_.end());
  DCHECK((*current_iterator)->HasNext());
  const Rule& to_return = (*current_iterator)->Next();
  if (!(*current_iterator)->HasNext())
    iterators_.erase(current_iterator);
  return to_return;
}

}  // namespace content_settings
