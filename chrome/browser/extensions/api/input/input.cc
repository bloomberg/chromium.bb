// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input/input.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/events/event.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/keyboard/keyboard_util.h"
#endif

namespace {

const char kNotYetImplementedError[] =
    "API is not implemented on this platform.";

}  // namespace

namespace extensions {

bool SendKeyboardEventInputFunction::RunImpl() {
#if defined(USE_ASH)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<ui::KeyEvent> event(
      keyboard::KeyEventFromArgs(args_.get(), &error_));
  if (!event)
    return false;

  ash::Shell::GetActiveRootWindow()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      event.get());

  return true;
#endif
  error_ = kNotYetImplementedError;
  return false;
}

InputAPI::InputAPI(Profile* profile) {
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<SendKeyboardEventInputFunction>();
}

InputAPI::~InputAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<InputAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<InputAPI>* InputAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
