// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_

#include <string>

typedef struct _GdkPixbuf GdkPixbuf;

class CommandLine;
class SkBitmap;

namespace base {
class Environment;
}

namespace libgtk2ui {

void GtkInitFromCommandLine(const CommandLine& command_line);

// Returns the name of the ".desktop" file associated with our running process.
std::string GetDesktopName(base::Environment* env);

const SkBitmap GdkPixbufToImageSkia(GdkPixbuf* pixbuf);

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
