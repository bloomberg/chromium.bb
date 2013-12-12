// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_ISOLATE_DATA_H_
#define GIN_PER_ISOLATE_DATA_H_

#include <map>

#include "base/basictypes.h"
#include "gin/gin_export.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/v8.h"

namespace gin {

// There is one instance of PerIsolateData per v8::Isolate managed by Gin. This
// class stores all the Gin-related data that varies per isolate.
class GIN_EXPORT PerIsolateData {
 public:
  explicit PerIsolateData(v8::Isolate* isolate);
  ~PerIsolateData();

  static PerIsolateData* From(v8::Isolate* isolate);

  // Each isolate is associated with a collection of v8::ObjectTemplates and
  // v8::FunctionTemplates. Typically these template objects are created
  // lazily.
  void SetObjectTemplate(WrapperInfo* info,
                         v8::Local<v8::ObjectTemplate> object_template);
  void SetFunctionTemplate(WrapperInfo* info,
                           v8::Local<v8::FunctionTemplate> function_template);

  // These are low-level functions for retrieving object or function templates
  // stored in this object. Because these templates are often created lazily,
  // most clients should call higher-level functions that know how to populate
  // these templates if they haven't already been created.
  v8::Local<v8::ObjectTemplate> GetObjectTemplate(WrapperInfo* info);
  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(WrapperInfo* info);

  v8::Isolate* isolate() { return isolate_; }

 private:
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::ObjectTemplate> > ObjectTemplateMap;
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::FunctionTemplate> > FunctionTemplateMap;

  // PerIsolateData doesn't actually own |isolate_|. Instead, the isolate is
  // owned by the IsolateHolder, which also owns the PerIsolateData.
  v8::Isolate* isolate_;
  ObjectTemplateMap object_templates_;
  FunctionTemplateMap function_templates_;

  DISALLOW_COPY_AND_ASSIGN(PerIsolateData);
};

}  // namespace gin

#endif  // GIN_PER_ISOLATE_DATA_H_
