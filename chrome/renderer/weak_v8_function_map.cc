// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/renderer/weak_v8_function_map.h"

// Extra data to be passed to MakeWeak/RemoveFromMap to know which entry
// to remove from which map.
struct WeakV8FunctionMapData {
  base::WeakPtr<WeakV8FunctionMap> map;
  int key;
};

// Disposes of a callback function and its corresponding entry in the callback
// map, if that callback map is still alive.
static void RemoveFromMap(v8::Persistent<v8::Value> context,
                                  void* data) {
  WeakV8FunctionMapData* map_data =
      static_cast<WeakV8FunctionMapData*>(data);
  if (map_data->map)
    map_data->map->Remove(map_data->key);
  delete map_data;
  context.Dispose();
}

WeakV8FunctionMap::WeakV8FunctionMap()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

WeakV8FunctionMap::~WeakV8FunctionMap() {}


void WeakV8FunctionMap::Add(int key,
                            v8::Local<v8::Function> callback_function) {
  WeakV8FunctionMapData* map_data = new WeakV8FunctionMapData();
  map_data->key = key;
  map_data->map = weak_ptr_factory_.GetWeakPtr();
  v8::Persistent<v8::Function> wrapper =
      v8::Persistent<v8::Function>::New(callback_function);
  map_[key] = wrapper;
  wrapper.MakeWeak(map_data, RemoveFromMap);
}

v8::Persistent<v8::Function> WeakV8FunctionMap::Remove(int key) {
  WeakV8FunctionMap::Map::iterator i = map_.find(key);
  if (i == map_.end())
    return v8::Persistent<v8::Function>();
  v8::Persistent<v8::Function> callback = i->second;
  map_.erase(i);
  return callback;
}
