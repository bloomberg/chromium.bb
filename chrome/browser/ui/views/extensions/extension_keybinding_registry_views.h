// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"

class Extension;
class Profile;

namespace views {
class FocusManager;
}

// ExtensionKeybindingRegistryViews is a class that handles Views-specific
// implementation of the Extension Keybinding shortcuts (keyboard accelerators).
// Note: It handles regular extension commands (not browserAction and pageAction
// popups, which are handled elsewhere). This class registers the accelerators
// on behalf of the extensions and routes the commands to them via the
// BrowserEventRouter.
class ExtensionKeybindingRegistryViews
    : public extensions::ExtensionKeybindingRegistry,
      public ui::AcceleratorTarget {
 public:
  ExtensionKeybindingRegistryViews(Profile* profile,
                                   views::FocusManager* focus_manager);
  virtual ~ExtensionKeybindingRegistryViews();

  // Overridden from ui::AcceleratorTarget.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

 private:
  // Overridden from ExtensionKeybindingRegistry:
  virtual void AddExtensionKeybinding(const Extension* extension) OVERRIDE;
  virtual void RemoveExtensionKeybinding(const Extension* extension) OVERRIDE;

  // Weak pointer to the our profile. Not owned by us.
  Profile* profile_;

  // Weak pointer back to the focus manager to use to register and unregister
  // accelerators with. Not owned by us.
  views::FocusManager* focus_manager_;

  // Maps an accelerator to a string pair (extension id, command name) for
  // commands that have been registered. Unlike its GTK counterpart, this map
  // contains no registration for pageAction and browserAction commands.
  typedef std::map< ui::Accelerator,
                    std::pair<std::string, std::string> > EventTargets;
  EventTargets event_targets_;

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistryViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
