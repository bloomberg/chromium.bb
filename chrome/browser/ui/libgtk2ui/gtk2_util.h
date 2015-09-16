// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_

#include <gtk/gtk.h>
#include <string>

#include "ui/native_theme/native_theme.h"

class SkBitmap;

namespace aura {
class Window;
}

namespace base {
class CommandLine;
class Environment;
}

namespace ui {
class Accelerator;
}

namespace libgtk2ui {

void GtkInitFromCommandLine(const base::CommandLine& command_line);

// Returns the name of the ".desktop" file associated with our running process.
std::string GetDesktopName(base::Environment* env);

guint GetGdkKeyCodeForAccelerator(const ui::Accelerator& accelerator);

GdkModifierType GetGdkModifierForAccelerator(
    const ui::Accelerator& accelerator);

// Translates event flags into plaform independent event flags.
int EventFlagsFromGdkState(guint state);

// Style a GTK button as a BlueButton
void TurnButtonBlue(GtkWidget* button);

// Sets |dialog| as transient for |parent|, which will keep it on top and center
// it above |parent|. Do nothing if |parent| is NULL.
void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent);

// Gets the transient parent aura window for |dialog|.
aura::Window* GetAuraTransientParent(GtkWidget* dialog);

// Clears the transient parent for |dialog|.
void ClearAuraTransientParent(GtkWidget* dialog);

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UTIL_H_
