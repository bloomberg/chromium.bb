// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/extension_icon_image.h"

class ExtensionAction;
class ExtensionIconSet;
class Profile;

namespace extensions {
class Extension;
}

// Used to get an icon to be used in the UI for an extension action.
// If the extension action icon is the default icon defined in the extension's
// manifest, it is loaded using extensions::IconImage. This icon can be loaded
// asynchronously. The factory observes underlying IconImage and notifies its
// own observer when the icon image changes.
class ExtensionActionIconFactory : public extensions::IconImage::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when the underlying icon image changes.
    virtual void OnIconUpdated() = 0;
  };

  // Observer should outlive this.
  ExtensionActionIconFactory(Profile* profile,
                             const extensions::Extension* extension,
                             const ExtensionAction* action,
                             Observer* observer);
  virtual ~ExtensionActionIconFactory();

  // extensions::IconImage override.
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

  // Gets the extension action icon for the tab.
  // If there is an icon set using |SetIcon|, that icon is returned.
  // Else, if there is a default icon set for the extension action, the icon is
  // created using IconImage. Observer is triggered wheniever the icon gets
  // updated.
  // Else, extension favicon is returned.
  // In all cases, action's attention and animation icon transformations are
  // applied on the icon.
  gfx::Image GetIcon(int tab_id);

 private:
  // Gets the icon that should be returned by |GetIcon| (without the attention
  // and animation transformations).
  gfx::ImageSkia GetBaseIconFromAction(int tab_id);

  const extensions::Extension* extension_;
  const ExtensionAction* action_;
  Observer* observer_;
  // Underlying icon image for the default icon.
  scoped_ptr<extensions::IconImage> default_icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionIconFactory);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_
