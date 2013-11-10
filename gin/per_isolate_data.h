// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_ISOLATE_DATA_H_
#define GIN_PER_ISOLATE_DATA_H_

#include <map>

#include "base/basictypes.h"
#include "gin/wrapper_info.h"
#include "v8/include/v8.h"

namespace gin {

class PerIsolateData {
 public:
  explicit PerIsolateData(v8::Isolate* isolate);
  ~PerIsolateData();

  static PerIsolateData* From(v8::Isolate* isolate);

  void RegisterObjectTemplate(WrapperInfo* info,
                              v8::Local<v8::ObjectTemplate> object_template);

 private:
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::ObjectTemplate> > ObjectTemplateMap;

  v8::Isolate* isolate_;
  ObjectTemplateMap object_templates_;

  DISALLOW_COPY_AND_ASSIGN(PerIsolateData);
};

}  // namespace gin

#endif  // GIN_PER_ISOLATE_DATA_H_
