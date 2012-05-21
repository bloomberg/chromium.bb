// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/page_actions_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_action.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

PageActionsCustomBindings::PageActionsCustomBindings(
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(extension_dispatcher) {
  RouteStaticFunction("GetCurrentPageActions", &GetCurrentPageActions);
}

// static
v8::Handle<v8::Value> PageActionsCustomBindings::GetCurrentPageActions(
    const v8::Arguments& args) {
  PageActionsCustomBindings* self =
      GetFromArguments<PageActionsCustomBindings>(args);
  std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
  CHECK(!extension_id.empty());
  const Extension* extension =
      self->extension_dispatcher_->extensions()->GetByID(extension_id);
  CHECK(extension);

  v8::Local<v8::Array> page_action_vector = v8::Array::New();
  if (extension->page_action()) {
    std::string id = extension->page_action()->id();
    page_action_vector->Set(v8::Integer::New(0),
                            v8::String::New(id.c_str(), id.size()));
  }

  return page_action_vector;
}

}  // extensions
