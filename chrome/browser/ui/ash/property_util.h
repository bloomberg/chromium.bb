// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROPERTY_UTIL_H_
#define CHROME_BROWSER_UI_ASH_PROPERTY_UTIL_H_

#include "base/strings/string16.h"
#include "ui/aura/window.h"

// These utility functions set ui::Window properties when running in mash, or
// they set aura::Window properties when running in classic ash (without mus).

namespace property_util {

// Sets the |value| of the given window |property|; supports limited properties.
void SetIntProperty(aura::Window* window,
                    const aura::WindowProperty<int>* property,
                    int value);

// Set the aura::Window title value or the ui::Window title property value.
void SetTitle(aura::Window* window, const base::string16& value);

}  // namespace property_util

#endif  // CHROME_BROWSER_UI_ASH_PROPERTY_UTIL_H_
