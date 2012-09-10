// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_COCOA_H_

#include <map>
#include <string>
#include <utility>

#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace content {
  struct NativeWebKeyboardEvent;
}
namespace extensions {
  class Extension;
}

// The ExtensionKeybindingRegistryCocoa is the Cocoa specialization of the
// ExtensionKeybindingRegistry class that handles turning keyboard shortcuts
// into events that get sent to the extension.

// ExtensionKeybindingRegistryCocoa is a class that handles Cocoa-specific
// implemenation of the Extension Commands shortcuts (keyboard accelerators).
// It also routes the events to the intended recipient (ie. to the browser
// action button in case of browser action commands).
class ExtensionKeybindingRegistryCocoa
    : public extensions::ExtensionKeybindingRegistry {
 public:
  ExtensionKeybindingRegistryCocoa(Profile* profile,
                                   gfx::NativeWindow window,
                                   ExtensionFilter extension_filter);
  virtual ~ExtensionKeybindingRegistryCocoa();

  static void set_shortcut_handling_suspended(bool suspended) {
    shortcut_handling_suspended_ = suspended;
  }
  static bool shortcut_handling_suspended() {
    return shortcut_handling_suspended_;
  }

  // For a given keyboard |event|, see if a known Extension Command registration
  // exists and route the event to it. Returns true if the event was handled,
  // false otherwise.
  bool ProcessKeyEvent(const content::NativeWebKeyboardEvent& event);

 protected:
  // Overridden from ExtensionKeybindingRegistry:
  virtual void AddExtensionKeybinding(
      const extensions::Extension* extension,
      const std::string& command_name) OVERRIDE;
  virtual void RemoveExtensionKeybinding(
      const extensions::Extension* extension,
      const std::string& command_name) OVERRIDE;

 private:
  // Keeps track of whether shortcut handling is currently suspended. Shortcuts
  // are suspended briefly while capturing which shortcut to assign to an
  // extension command in the Config UI. If handling isn't suspended while
  // capturing then trying to assign Ctrl+F to a command would instead result
  // in the Find box opening.
  static bool shortcut_handling_suspended_;

  // Weak pointer to the our profile. Not owned by us.
  Profile* profile_;

  // The window we are associated with.
  gfx::NativeWindow window_;

  // Maps an accelerator to a string pair (extension id, command name) for
  // commands that have been registered. Unlike its Views counterpart, this map
  // contains browserAction and pageAction commands (but does not route those
  // events), so that we can query priority handlers in HasPriorityHandler(...).
  typedef std::map< ui::Accelerator,
                    std::pair<std::string, std::string> > EventTargets;
  EventTargets event_targets_;

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistryCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_COCOA_H_
