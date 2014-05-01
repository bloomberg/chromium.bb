// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_
#define APPS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_

#include "base/macros.h"
#include "extensions/renderer/default_dispatcher_delegate.h"

namespace apps {

// app_shell's implementation of DispatcherDelegate. This inherits the behavior
// of the default delegate while augmenting its own script resources and native
// native handlers.
class ShellDispatcherDelegate : public extensions::DefaultDispatcherDelegate {
 public:
  ShellDispatcherDelegate();
  virtual ~ShellDispatcherDelegate();

  // DispatcherDelegate implementation.
  virtual void RegisterNativeHandlers(
      extensions::Dispatcher* dispatcher,
      extensions::ModuleSystem* module_system,
      extensions::ScriptContext* context) OVERRIDE;
  virtual void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDispatcherDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_RENDERER_SHELL_DISPATCHER_DELEGATE_H_
