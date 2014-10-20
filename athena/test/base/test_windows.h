// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_TEST_WINDOWS_H_
#define ATHENA_TEST_BASE_TEST_WINDOWS_H_

#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/public/window_types.h"

namespace aura {
class Window;
class WindowDelegate;
}

namespace gfx {
class Rect;
}

namespace athena {
namespace test {

scoped_ptr<aura::Window> CreateNormalWindow(aura::WindowDelegate* delegate,
                                            aura::Window* parent);

scoped_ptr<aura::Window> CreateWindowWithType(aura::WindowDelegate* delegate,
                                              aura::Window* parent,
                                              ui::wm::WindowType window_type);

scoped_ptr<aura::Window> CreateTransientWindow(aura::WindowDelegate* delegate,
                                               aura::Window* transient_parent,
                                               ui::ModalType modal_type,
                                               bool top_most);

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_BASE_TEST_WINDOWS_H_
