// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_persistence.h"

namespace chromeos {
namespace input_method {

namespace {
InputMethodManager* g_input_method_manager = NULL;
InputMethodPersistence* g_input_method_persistence = NULL;
BrowserStateMonitor* g_browser_state_monitor = NULL;
}  // namespace

void OnSessionStateChange(InputMethodManagerImpl* input_method_manager_impl,
                          InputMethodPersistence* input_method_persistence,
                          InputMethodManager::State new_state) {
  input_method_persistence->OnSessionStateChange(new_state);
  input_method_manager_impl->SetState(new_state);
}

void Initialize() {
  DCHECK(!g_input_method_manager);

  InputMethodManagerImpl* impl = new InputMethodManagerImpl(
      scoped_ptr<InputMethodDelegate>(new InputMethodDelegateImpl));
  impl->Init();
  g_input_method_manager = impl;
  g_input_method_persistence =
      new InputMethodPersistence(g_input_method_manager);
  g_browser_state_monitor = new BrowserStateMonitor(
      base::Bind(&OnSessionStateChange, impl, g_input_method_persistence));

  DVLOG(1) << "InputMethodManager initialized";
}

void InitializeForTesting(InputMethodManager* mock_manager) {
  DCHECK(!g_input_method_manager);
  g_input_method_manager = mock_manager;
  DVLOG(1) << "InputMethodManager for testing initialized";
}

void Shutdown() {
  delete g_browser_state_monitor;
  g_browser_state_monitor = NULL;

  delete g_input_method_persistence;
  g_input_method_persistence = NULL;

  delete g_input_method_manager;
  g_input_method_manager = NULL;

  DVLOG(1) << "InputMethodManager shutdown";
}

InputMethodManager* GetInputMethodManager() {
  DCHECK(g_input_method_manager);
  return g_input_method_manager;
}

}  // namespace input_method
}  // namespace chromeos
