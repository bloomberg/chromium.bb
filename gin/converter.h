// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_CONVERTER_H_
#define GIN_CONVERTER_H_

#include <string>
#include <vector>

#include "v8/include/v8.h"

namespace gin {

template<typename T>
struct Converter {};

template<>
struct Converter<bool> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    bool val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     bool* out);
};

template<>
struct Converter<int32_t> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    int32_t val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     int32_t* out);
};

template<>
struct Converter<uint32_t> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    uint32_t val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     uint32_t* out);
};

template<>
struct Converter<double> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    double val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     double* out);
};

template<>
struct Converter<std::string> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const std::string& val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     std::string* out);
};

template<>
struct Converter<v8::Handle<v8::Function> > {
  static bool FromV8(v8::Handle<v8::Value> val,
                     v8::Handle<v8::Function>* out);
};

template<>
struct Converter<v8::Handle<v8::Object> > {
  static v8::Handle<v8::Value> ToV8(v8::Handle<v8::Object> val);
  static bool FromV8(v8::Handle<v8::Value> val,
                     v8::Handle<v8::Object>* out);
};

template<typename T>
struct Converter<std::vector<T> > {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const std::vector<T>& val) {
    v8::Handle<v8::Array> result(v8::Array::New(static_cast<int>(val.size())));
    for (size_t i = 0; i < val.size(); ++i) {
      result->Set(static_cast<int>(i), Converter<T>::ToV8(isolate, val[i]));
    }
    return result;
  }

  static bool FromV8(v8::Handle<v8::Value> val,
                     std::vector<T>* out) {
    if (!val->IsArray())
      return false;

    std::vector<T> result;
    v8::Handle<v8::Array> array(v8::Handle<v8::Array>::Cast(val));
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      T item;
      if (!Converter<T>::FromV8(array->Get(i), &item))
        return false;
      result.push_back(item);
    }

    out->swap(result);
    return true;
  }
};

// Convenience functions that deduce T.
template<typename T>
v8::Handle<v8::Value> ConvertToV8(v8::Isolate* isolate,
                                  T input) {
  return Converter<T>::ToV8(isolate, input);
}

inline v8::Handle<v8::String> StringToV8(
    v8::Isolate* isolate, std::string input) {
  return ConvertToV8(isolate, input).As<v8::String>();
}

template<typename T>
bool ConvertFromV8(v8::Handle<v8::Value> input, T* result) {
  return Converter<T>::FromV8(input, result);
}

}  // namespace gin

#endif  // GIN_CONVERTER_H_
