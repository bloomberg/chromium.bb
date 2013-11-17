// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_CONTEXT_DATA_H_
#define GIN_PER_CONTEXT_DATA_H_

#include <vector>
#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace gin {

class ContextSupplement {
 public:
  ContextSupplement();
  virtual ~ContextSupplement();

  virtual void Detach(v8::Handle<v8::Context> context) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextSupplement);
};

class PerContextData {
 public:
  explicit PerContextData(v8::Handle<v8::Context> context);
  ~PerContextData();

  // Can return NULL after the ContextHolder has detached from context.
  static PerContextData* From(v8::Handle<v8::Context>);
  void Detach(v8::Handle<v8::Context> context);

  // Takes ownership of the supplement.
  void AddSupplement(ContextSupplement* supplement);

 private:
  typedef std::vector<ContextSupplement*> SuplementVector;

  // Owning reference.
  SuplementVector supplements_;

  DISALLOW_COPY_AND_ASSIGN(PerContextData);
};

}  // namespace gin

#endif  // GIN_PER_CONTEXT_DATA_H_
