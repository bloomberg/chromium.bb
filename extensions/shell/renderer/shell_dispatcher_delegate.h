// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_

#include "base/macros.h"
#include "extensions/renderer/default_dispatcher_delegate.h"

namespace extensions {

// app_shell's implementation of DispatcherDelegate. This inherits the behavior
// of the default delegate while augmenting its own script resources and native
// native handlers.
class ShellDispatcherDelegate : public DefaultDispatcherDelegate {
 public:
  ShellDispatcherDelegate();
  virtual ~ShellDispatcherDelegate();

  // DispatcherDelegate implementation.
  virtual void RegisterNativeHandlers(Dispatcher* dispatcher,
                                      ModuleSystem* module_system,
                                      ScriptContext* context) OVERRIDE;
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDispatcherDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_
