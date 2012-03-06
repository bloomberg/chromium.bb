// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/accelerators/accelerator.h"

class Extension;
class Profile;

namespace views {
class FocusManager;
}

// The ExtensionKeybindingRegistry is a class that handles keyboard accelerators
// for regular extension commands (other than displaying browserAction and
// pageAction popups). It registers all those accelerators on behalf of the
// extensions and routes the commands to them via the BrowserEventRouter.
class ExtensionKeybindingRegistry : ui::AcceleratorTarget,
                                    public content::NotificationObserver {
 public:
  ExtensionKeybindingRegistry(Profile* profile,
                              views::FocusManager* focus_manager);
  virtual ~ExtensionKeybindingRegistry();

  // Overridden from ui::AcceleratorTarget.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void AddExtensionKeybinding(const Extension* extension);
  void RemoveExtensionKeybinding(const Extension* extension);

  // Whether to ignore this command.
  bool ShouldIgnoreCommand(const std::string& command) const;

  // Weak pointer back to parent profile.
  Profile* profile_;

  // Weak pointer back to the focus manager to use to register and unregister
  // accelerators with.
  views::FocusManager* focus_manager_;

  // Maps an accelerator to a string pair (extension id, command name).
  typedef std::map< ui::Accelerator,
                    std::pair<std::string, std::string> > EventTargets;
  EventTargets event_targets_;

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistry);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
