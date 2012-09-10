// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_GTK_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

namespace extensions {
class Extension;
}

typedef struct _GtkAccelGroup GtkAccelGroup;
typedef struct _GdkEventKey GdkEventKey;

// The ExtensionKeybindingRegistryGtk is the GTK specialization of the
// ExtensionKeybindingRegistry class that handles turning keyboard shortcuts
// into events that get sent to the extension.

// ExtensionKeybindingRegistryGtk is a class that handles GTK-specific
// implemenation of the Extension Keybinding shortcuts (keyboard accelerators).
// Note: It keeps track of browserAction and pageAction commands, but does not
// route the events to them -- that is handled elsewhere. This class registers
// the accelerators on behalf of the extensions and routes the commands to them
// via the BrowserEventRouter.
class ExtensionKeybindingRegistryGtk
    : public extensions::ExtensionKeybindingRegistry {
 public:
  ExtensionKeybindingRegistryGtk(Profile* profile,
                                 gfx::NativeWindow window,
                                 ExtensionFilter extension_filter);
  virtual ~ExtensionKeybindingRegistryGtk();

  static void set_shortcut_handling_suspended(bool suspended) {
    shortcut_handling_suspended_ = suspended;
  }
  static bool shortcut_handling_suspended() {
    return shortcut_handling_suspended_;
  }

  // Whether this class has any registered keyboard shortcuts that correspond
  // to |event|.
  gboolean HasPriorityHandler(const GdkEventKey* event) const;

 protected:
  // Overridden from ExtensionKeybindingRegistry:
  virtual void AddExtensionKeybinding(
      const extensions::Extension* extension,
      const std::string& command_name) OVERRIDE;
  virtual void RemoveExtensionKeybinding(
      const extensions::Extension* extension,
      const std::string& command_name) OVERRIDE;

 private:
  // The accelerator handler for when the extension command shortcuts are
  // struck.
  CHROMEG_CALLBACK_3(ExtensionKeybindingRegistryGtk, gboolean, OnGtkAccelerator,
                     GtkAccelGroup*, GObject*, guint, GdkModifierType);

  // Keeps track of whether shortcut handling is currently suspended. Shortcuts
  // are suspended briefly while capturing which shortcut to assign to an
  // extension command in the Config UI. If handling isn't suspended while
  // capturing then trying to assign Ctrl+F to a command would instead result
  // in the Find box opening.
  static bool shortcut_handling_suspended_;

  // Weak pointer to the our profile. Not owned by us.
  Profile* profile_;

  // The browser window the accelerators are associated with.
  gfx::NativeWindow window_;

  // The accelerator group used to handle accelerators, owned by this object.
  GtkAccelGroup* accel_group_;

  // Maps a GTK accelerator to a string pair (extension id, command name) for
  // commands that have been registered. Unlike its Views counterpart, this map
  // contains browserAction and pageAction commands (but does not route those
  // events), so that we can query priority handlers in HasPriorityHandler(...).
  typedef std::map< ui::AcceleratorGtk,
                    std::pair<std::string, std::string> > EventTargets;
  EventTargets event_targets_;

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistryGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_GTK_H_
