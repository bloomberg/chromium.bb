// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_application_menu_item.h"

namespace ash {

ShelfApplicationMenuItem::ShelfApplicationMenuItem(const base::string16 title,
                                                   const gfx::Image* icon)
    : title_(title), icon_(icon ? gfx::Image(*icon) : gfx::Image()) {}

ShelfApplicationMenuItem::~ShelfApplicationMenuItem() {}

void ShelfApplicationMenuItem::Execute(int event_flags) {}

}  // namespace ash
