// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a builders for DictionaryValue and ListValue.  These
// aren't specific to extensions and could move up to base/ if there's interest
// from other sub-projects.
//
// The general pattern is to write:
//
//  scoped_ptr<BuiltType> result(FooBuilder()
//                               .Set(args)
//                               .Set(args)
//                               .Build());
//
// For methods that take other built types, you can pass the builder directly
// to the setter without calling Build():
//
// DictionaryBuilder().Set("key", ListBuilder()
//                         .Append("foo").Append("bar") /* No .Build() */);
//
// Because of limitations in C++03, and to avoid extra copies, you can't pass a
// just-constructed Builder into another Builder's method without setting at
// least 1 field.
//
// The Build() method invalidates its builder, and returns ownership of the
// built value.
//
// These objects are intended to be used as temporaries rather than stored
// anywhere, so the use of non-const reference parameters is likely to cause
// less confusion than usual.

#ifndef CHROME_COMMON_EXTENSIONS_VALUE_BUILDER_H_
#define CHROME_COMMON_EXTENSIONS_VALUE_BUILDER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"

namespace extensions {

class ListBuilder;

class DictionaryBuilder {
 public:
  DictionaryBuilder();
  explicit DictionaryBuilder(const base::DictionaryValue& init);
  ~DictionaryBuilder();

  // Can only be called once, after which it's invalid to use the builder.
  scoped_ptr<base::DictionaryValue> Build() { return dict_.Pass(); }

  DictionaryBuilder& Set(const std::string& path, int in_value);
  DictionaryBuilder& Set(const std::string& path, double in_value);
  DictionaryBuilder& Set(const std::string& path, const std::string& in_value);
  DictionaryBuilder& Set(const std::string& path, const string16& in_value);
  DictionaryBuilder& Set(const std::string& path, DictionaryBuilder& in_value);
  DictionaryBuilder& Set(const std::string& path, ListBuilder& in_value);

  // Named differently because overload resolution is too eager to
  // convert implicitly to bool.
  DictionaryBuilder& SetBoolean(const std::string& path, bool in_value);

 private:
  scoped_ptr<base::DictionaryValue> dict_;
};

class ListBuilder {
 public:
  ListBuilder();
  explicit ListBuilder(const base::ListValue& init);
  ~ListBuilder();

  // Can only be called once, after which it's invalid to use the builder.
  scoped_ptr<base::ListValue> Build() { return list_.Pass(); }

  ListBuilder& Append(int in_value);
  ListBuilder& Append(double in_value);
  ListBuilder& Append(const std::string& in_value);
  ListBuilder& Append(const string16& in_value);
  ListBuilder& Append(DictionaryBuilder& in_value);
  ListBuilder& Append(ListBuilder& in_value);

  // Named differently because overload resolution is too eager to
  // convert implicitly to bool.
  ListBuilder& AppendBoolean(bool in_value);

 private:
  scoped_ptr<base::ListValue> list_;
};

} // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_VALUE_BUILDER_H_
