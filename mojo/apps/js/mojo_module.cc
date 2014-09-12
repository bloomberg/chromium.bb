// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/mojo_module.h"

#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "mojo/apps/js/js_app.h"
#include "mojo/apps/js/mojo_module.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/common/data_pipe_utils.h"

namespace mojo {
namespace apps {

namespace {

gin::WrapperInfo g_wrapper_info = {gin::kEmbedderNativeGin};

}  // namespace

const char Mojo::kModuleName[] = "mojo";

v8::Local<v8::Value> Mojo::GetModule(JSApp* js_app, v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ =
      data->GetObjectTemplate(&g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
                .SetMethod("connectToService",
                           base::Bind(&JSApp::ConnectToService,
                                      base::Unretained(js_app)))
                .SetMethod("quit",
                           base::Bind(&JSApp::Quit, base::Unretained(js_app)))
                .Build();
    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ->NewInstance();
}

}  // namespace apps
}  // namespace mojo
