// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/playback_extension.h"

#include "v8/include/v8.h"

const char kPlaybackExtensionName[] = "v8/PlaybackMode";

namespace extensions_v8 {

v8::Extension* PlaybackExtension::Get() {
  v8::Extension* extension = new v8::Extension(
      kPlaybackExtensionName,
      "(function () {"
      "  var orig_date = Date;"
      "  var x = 0;"
      "  var time_seed = 1204251968254;"
      "  Math.random = function() {"
      "    x += .1;"
      "    return (x % 1);"
      "  };"
      "  Date = function() {"
      "    if (this instanceof Date) {"
      "      switch (arguments.length) {"
      "        case 0: return new orig_date(time_seed += 50);"
      "        case 1: return new orig_date(arguments[0]);"
      "        default: return new orig_date(arguments[0], arguments[1],"
      "            arguments.length >= 3 ? arguments[2] : 1,"
      "            arguments.length >= 4 ? arguments[3] : 0,"
      "            arguments.length >= 5 ? arguments[4] : 0,"
      "            arguments.length >= 6 ? arguments[5] : 0,"
      "            arguments.length >= 7 ? arguments[6] : 0);"
      "      }"
      "    }"
      "    return new Date().toString();"
      "  };"
      "  Date.__proto__ = orig_date;"
      "  Date.prototype.constructor = Date;"
      "  orig_date.now = function() {"
      "    return new Date().getTime();"
      "  };"
      "})()");
    return extension;
}

}  // namespace extensions_v8
