// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_PROPERTY_UTIL_H_
#define ASH_MUS_PROPERTY_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace aura {
class PropertyConverter;
class Window;
}

namespace gfx {
class Rect;
class Size;
}

namespace ui {
namespace mojom {
enum class WindowType;
}
}

namespace ash {
namespace mus {

// Functions for extracting properties that are used at a Window creation time.
// When an aura::Window is created at the request of a client an initial set of
// properties is supplied to allow the WindowManager (ash) to configure the
// newly created window. Not all of these properties need be persisted, some are
// used solely to configure the window. This file contains the functions used
// to extract these properties.
// Long lived properties are converted and stored as properties on the
// associated aura::Window. See aura::PropertyConverter for this set of
// properties.

using InitProperties = std::map<std::string, std::vector<uint8_t>>;

// Returns the kDisplayId_InitProperty if present, otherwise
// kInvalidDisplayID.
int64_t GetInitialDisplayId(const InitProperties& properties);

// If |window| has the |kContainerId_InitProperty| set as a property, then
// the value of |kContainerId_InitProperty| is set in |container_id| and true
// is returned. Otherwise false is returned.
bool GetInitialContainerId(const InitProperties& properties, int* container_id);

bool GetInitialBounds(const InitProperties& properties, gfx::Rect* bounds);

bool GetWindowPreferredSize(const InitProperties& properties, gfx::Size* size);

bool ShouldRemoveStandardFrame(const InitProperties& properties);

bool ShouldEnableImmersive(const InitProperties& properties);

// Applies |properties| to |window| using |property_converter|.
void ApplyProperties(
    aura::Window* window,
    aura::PropertyConverter* property_converter,
    const std::map<std::string, std::vector<uint8_t>>& properties);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_PROPERTY_UTIL_H_
