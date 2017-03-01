// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_STORAGE_AREA_H_
#define EXTENSIONS_RENDERER_STORAGE_AREA_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
class APIRequestHandler;
class APITypeReferenceMap;

// Implementation of the storage.StorageArea custom type used in the
// chrome.storage API.
class StorageArea {
 public:
  StorageArea(APIRequestHandler* request_handler,
              const APITypeReferenceMap* type_refs,
              const std::string& name);
  ~StorageArea();

  // Creates a StorageArea object for the given context and property name.
  static v8::Local<v8::Object> CreateStorageArea(
      v8::Local<v8::Context> context,
      const std::string& property_name,
      APIRequestHandler* request_handler,
      APIEventHandler* event_handler,
      APITypeReferenceMap* type_refs);

  void HandleFunctionCall(const std::string& method_name,
                          gin::Arguments* arguments);

 private:
  APIRequestHandler* request_handler_;

  const APITypeReferenceMap* type_refs_;

  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(StorageArea);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_STORAGE_AREA_H_
