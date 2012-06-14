// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
#pragma once

class CommandLine;

namespace libgtk2ui {

void GtkInitFromCommandLine(const CommandLine& command_line);

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
