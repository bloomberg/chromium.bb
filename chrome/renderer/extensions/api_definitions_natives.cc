// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api_definitions_natives.h"

#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/renderer/extensions/user_script_slave.h"

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(extension_dispatcher) {
  RouteFunction("GetExtensionAPIDefinition",
                base::Bind(&ApiDefinitionsNatives::GetExtensionAPIDefinition,
                           base::Unretained(this)));
}

v8::Handle<v8::Value> ApiDefinitionsNatives::GetExtensionAPIDefinition(
    const v8::Arguments& args) {
  ChromeV8Context* v8_context =
      extension_dispatcher()->v8_context_set().GetCurrent();
  CHECK(v8_context);

  // TODO(kalman): This is being calculated twice, first in
  // ExtensionDispatcher then again here. It might as well be a property of
  // ChromeV8Context, however, this would require making ChromeV8Context take
  // an Extension rather than an extension ID.  In itself this is fine,
  // however it does not play correctly with the "IsTestExtensionId" checks.
  // We need to remove that first.
  scoped_ptr<std::set<std::string> > apis;

  const std::string& extension_id = v8_context->extension_id();
  if (extension_dispatcher()->IsTestExtensionId(extension_id)) {
    apis.reset(new std::set<std::string>());
    // The minimal set of APIs that tests need.
    apis->insert("extension");
  } else {
    apis = ExtensionAPI::GetSharedInstance()->GetAPIsForContext(
        v8_context->context_type(),
        extension_dispatcher()->extensions()->GetByID(extension_id),
        UserScriptSlave::GetDataSourceURLForFrame(v8_context->web_frame()));
  }

  return extension_dispatcher()->v8_schema_registry()->GetSchemas(*apis);
}

}  // namespace extensions
