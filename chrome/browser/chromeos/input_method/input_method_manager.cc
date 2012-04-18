// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager.h"

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

namespace chromeos {
namespace input_method {

namespace {
InputMethodManager* g_input_method_manager = NULL;
}  // namespace

// static
void InputMethodManager::Initialize() {
  DCHECK(!g_input_method_manager);
  InputMethodManagerImpl* impl = new InputMethodManagerImpl;
  impl->Init();
  g_input_method_manager = impl;
  DVLOG(1) << "InputMethodManager initialized";
}

// static
void InputMethodManager::InitializeForTesting(
    InputMethodManager* mock_manager) {
  DCHECK(!g_input_method_manager);
  g_input_method_manager = mock_manager;
  DVLOG(1) << "InputMethodManager for testing initialized";
}

// static
void InputMethodManager::Shutdown() {
  delete g_input_method_manager;
  g_input_method_manager = NULL;
  DVLOG(1) << "InputMethodManager shutdown";
}

// static
InputMethodManager* InputMethodManager::GetInstance() {
  DCHECK(g_input_method_manager);
  return g_input_method_manager;
}

}  // namespace input_method
}  // namespace chromeos
