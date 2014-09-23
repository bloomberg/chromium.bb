// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "extensions/common/features/feature.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
}

namespace extensions {
class Dispatcher;
class Extension;
class ModuleSystem;
class ResourceBundleSourceMap;
class ScriptContext;
class URLPatternSet;

// Base class and default implementation for an extensions::Dispacher delegate.
// DispatcherDelegate can be used to override and extend the behavior of the
// extensions system's renderer side.
class DispatcherDelegate {
 public:
  virtual ~DispatcherDelegate() {}

  // Creates a new ScriptContext for a given v8 context.
  virtual scoped_ptr<ScriptContext> CreateScriptContext(
      const v8::Handle<v8::Context>& v8_context,
      blink::WebFrame* frame,
      const Extension* extension,
      Feature::Context context_type,
      const Extension* effective_extension,
      Feature::Context effective_context_type) = 0;

  // Initializes origin permissions for a newly created extension context.
  virtual void InitOriginPermissions(const Extension* extension,
                                     bool is_extension_active) {}

  // Includes additional native handlers in a ScriptContext's ModuleSystem.
  virtual void RegisterNativeHandlers(Dispatcher* dispatcher,
                                      ModuleSystem* module_system,
                                      ScriptContext* context) {}

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) {}

  // Requires additional modules within an extension context's module system.
  virtual void RequireAdditionalModules(ScriptContext* context,
                                        bool is_within_platform_app) {}

  // Allows the delegate to respond to an updated set of active extensions in
  // the Dispatcher.
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extension_ids) {}

  // Sets the current Chrome channel.
  // TODO(rockot): This doesn't belong in a generic extensions system interface.
  // See http://crbug.com/368431.
  virtual void SetChannel(int channel) {}

  // Clears extension permissions specific to a given tab.
  // TODO(rockot): This doesn't belong in a generic extensions system interface.
  // See http://crbug.com/368431.
  virtual void ClearTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      int tab_id,
      const std::vector<std::string>& extension_ids) {}

  // Updates extension permissions specific to a given tab.
  // TODO(rockot): This doesn't belong in a generic extensions system interface.
  // See http://crbug.com/368431.
  virtual void UpdateTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      const GURL& url,
      int tab_id,
      const std::string& extension_id,
      const extensions::URLPatternSet& origin_set) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
