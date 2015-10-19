// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TYPES_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TYPES_H_

#include <stdint.h>

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace mus {

// Used to identify windows and change ids.
typedef uint32_t Id;

// Used to identify a connection as well as a connection specific window id. For
// example, the Id for a window consists of the ConnectionSpecificId of the
// connection and the ConnectionSpecificId of the window.
typedef uint16_t ConnectionSpecificId;

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TYPES_H_
