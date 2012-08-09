// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"

// Arbitrary number just to leave enough space for menu IDs
// that show before extensions. Like "Bookmark this page", "Send tab to device"
// and so on. They could have any IDs < kFirstExtensionCommandId.
static const int kFirstExtensionCommandId = 1000;

////////////////////////////////////////////////////////////////////////////////
// ActionBoxMenuModel

ActionBoxMenuModel::ActionBoxMenuModel(ExtensionService* extension_service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(NULL)),
      extension_service_(extension_service) {
  // Adds extensions to the model.
  int command_id = kFirstExtensionCommandId;
  const extensions::ExtensionList& action_box_items = action_box_menu_items();
  for (size_t i = 0; i < action_box_items.size(); ++i) {
    const extensions::Extension* extension = action_box_items[i];
    AddItem(command_id, UTF8ToUTF16(extension->name()));
    id_to_extension_id_map_[command_id++] = extension->id();
  }
}

ActionBoxMenuModel::~ActionBoxMenuModel() {
}

void ActionBoxMenuModel::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
}
