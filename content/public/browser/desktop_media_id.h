// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"

#if defined(USE_AURA)
namespace aura {
class Window;
}  // namespace aura
#endif  // defined(USE_AURA)

namespace content {

// Type used to identify desktop media sources. It's converted to string and
// stored in MediaStreamRequest::requested_video_device_id.
struct CONTENT_EXPORT DesktopMediaID {
 public:
  enum Type {
    TYPE_NONE,
    TYPE_SCREEN,
    TYPE_WINDOW,
  };

  typedef intptr_t Id;

  // Represents an "unset" value for either |id| or |aura_id|.
  static const Id kNullId = 0;

#if defined(USE_AURA)
  // Assigns integer identifier to the |window| and returns its DesktopMediaID.
  static DesktopMediaID RegisterAuraWindow(Type type, aura::Window* window);

  // Returns the Window that was previously registered using
  // RegisterAuraWindow(), else nullptr.
  static aura::Window* GetAuraWindowById(const DesktopMediaID& id);
#endif  // defined(USE_AURA)

  static DesktopMediaID Parse(const std::string& str);

  DesktopMediaID() = default;

  DesktopMediaID(Type type, Id id)
      : type(type),
        id(id) {
  }

  // Operators so that DesktopMediaID can be used with STL containers.
  bool operator<(const DesktopMediaID& other) const {
#if defined(USE_AURA)
    return (type < other.type ||
            (type == other.type &&
             (id < other.id || (id == other.id &&
                                aura_id < other.aura_id))));
#else
    return type < other.type || (type == other.type && id < other.id);
#endif
  }
  bool operator==(const DesktopMediaID& other) const {
#if defined(USE_AURA)
    return type == other.type && id == other.id && aura_id == other.aura_id;
#else
    return type == other.type && id == other.id;
#endif
  }

  bool is_null() { return type == TYPE_NONE; }

  std::string ToString();

  Type type = TYPE_NONE;

  // The IDs referring to the target native screen or window.  |id| will be
  // non-null if and only if it refers to a native screen/window.  |aura_id|
  // will be non-null if and only if it refers to an Aura window.  Note that is
  // it possible for both of these to be non-null, which means both IDs are
  // referring to the same logical window.
  Id id = kNullId;
#if defined(USE_AURA)
  // TODO(miu): Make this an int, after clean-up for http://crbug.com/513490.
  Id aura_id = kNullId;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_MEDIA_ID_H_
