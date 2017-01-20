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

#if GTK_MAJOR_VERSION > 2
// These constants are defined in gtk/gtkenums.h in Gtk3.12 or later.
// They are added here as a convenience to avoid version checks, and
// can be removed once the sysroot is switched from Wheezy to Jessie.
#define GTK_STATE_FLAG_LINK static_cast<GtkStateFlags>(1 << 9)
#define GTK_STATE_FLAG_VISITED static_cast<GtkStateFlags>(1 << 10)
#define GTK_STATE_FLAG_CHECKED static_cast<GtkStateFlags>(1 << 11)

// Returns true iff the runtime version of Gtk used meets
// |major|.|minor|.|micro|.
bool GtkVersionCheck(int major, int minor = 0, int micro = 0);

template <typename T>
class ScopedGObject {
 public:
  explicit ScopedGObject(T* obj) : obj_(obj) {
    // Increase the reference count of |obj_|, removing the floating
    // reference if it has one.
    g_object_ref_sink(obj_);
  }

  ScopedGObject(const ScopedGObject<T>& other) : obj_(other.obj_) {
    g_object_ref(obj_);
  }

  ScopedGObject(ScopedGObject<T>&& other) : obj_(other.obj_) {
    other.obj_ = nullptr;
  }

  ~ScopedGObject() {
    if (obj_)
      g_object_unref(obj_);
  }

  ScopedGObject<T>& operator=(const ScopedGObject<T>& other) {
    g_object_ref(other.obj_);
    g_object_unref(obj_);
    obj_ = other.obj_;
    return *this;
  }

  ScopedGObject<T>& operator=(ScopedGObject<T>&& other) {
    g_object_unref(obj_);
    obj_ = other.obj_;
    other.obj_ = nullptr;
    return *this;
  }

  operator T*() { return obj_; }

 private:
  T* obj_;
};

typedef ScopedGObject<GtkStyleContext> ScopedStyleContext;

// Parses |css_selector| into a GtkStyleContext.  The format is a
// sequence of whitespace-separated objects.  Each object may have at
// most one object name at the beginning of the string, and any number
// of '.'-prefixed classes and ':'-prefixed pseudoclasses.  An example
// is "GtkButton.button.suggested-action:hover:active".  The caller
// must g_object_unref() the returned context.
ScopedStyleContext GetStyleContextFromCss(const char* css_selector);

// Get the 'color' property from the style context created by
// GetStyleContextFromCss(|css_selector|).
SkColor GetFgColor(const char* css_selector);

// Renders a background from the style context created by
// GetStyleContextFromCss(|css_selector|) into a single pixel and
// returns the color.
SkColor GetBgColor(const char* css_selector);

// If there is a border, renders the border from the style context
// created by GetStyleContextFromCss(|css_selector|) into a single
// pixel and returns the color.  Otherwise returns kInvalidColor.
SkColor GetBorderColor(const char* css_selector);

// Get the color of the GtkSeparator specified by |css_selector|.
SkColor GetSeparatorColor(const char* css_selector);
#endif

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_
