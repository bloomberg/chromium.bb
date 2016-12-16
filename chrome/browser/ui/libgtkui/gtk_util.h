// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_
#define CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_

#include <gtk/gtk.h>
#include <string>

#include "ui/native_theme/native_theme.h"

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

namespace libgtkui {

extern const SkColor kInvalidColorIdColor;
extern const SkColor kURLTextColor;

// Generates the normal URL color, a green color used in unhighlighted URL
// text. It is a mix of |kURLTextColor| and the current text color.  Unlike the
// selected text color, it is more important to match the qualities of the
// foreground typeface color instead of taking the background into account.
SkColor NormalURLColor(SkColor foreground);

// Generates the selected URL color, a green color used on URL text in the
// currently highlighted entry in the autocomplete popup. It's a mix of
// |kURLTextColor|, the current text color, and the background color (the
// select highlight). It is more important to contrast with the background
// saturation than to look exactly like the foreground color.
SkColor SelectedURLColor(SkColor foreground, SkColor background);

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

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_
