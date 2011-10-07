// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEAK_V8_FUNCTION_MAP_H_
#define CHROME_RENDERER_WEAK_V8_FUNCTION_MAP_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "v8/include/v8.h"

// This class lets you keep a mapping of integer to weak reference to a v8
// function. The entry will automatically get removed from the map if the
// function gets garbage collected.
class WeakV8FunctionMap {
 public:
  WeakV8FunctionMap();
  ~WeakV8FunctionMap();

  // Adds |callback_function| to the map under |key|. The entry will be removed
  // from the map when the function is about to be GCed.
  void Add(int key, v8::Local<v8::Function> callback_function);

  // Removes and returns a entry from the map for |key|. If there was no entry
  // for |key|, the return value will return true for IsEmpty().
  v8::Persistent<v8::Function> Remove(int key);

 private:
  typedef std::map<int, v8::Persistent<v8::Function> > Map;
  Map map_;
  base::WeakPtrFactory<WeakV8FunctionMap> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WeakV8FunctionMap);
};

#endif  // CHROME_RENDERER_WEAK_V8_FUNCTION_MAP_H_
