// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_keybinding_registry_gtk.h"

#include "chrome/browser/extensions/api/commands/extension_command_service.h"
#include "chrome/browser/extensions/api/commands/extension_command_service_factory.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"

ExtensionKeybindingRegistryGtk::ExtensionKeybindingRegistryGtk(
    Profile* profile, gfx::NativeWindow window)
    : ExtensionKeybindingRegistry(profile),
      profile_(profile),
      window_(window),
      accel_group_(NULL) {
  Init();
}

ExtensionKeybindingRegistryGtk::~ExtensionKeybindingRegistryGtk() {
  if (accel_group_) {
    gtk_accel_group_disconnect(accel_group_,
                               NULL);  // Remove all closures.
    event_targets_.clear();

    gtk_window_remove_accel_group(window_, accel_group_);
    g_object_unref(accel_group_);
    accel_group_ = NULL;
  }
}

gboolean ExtensionKeybindingRegistryGtk::HasPriorityHandler(
    const GdkEventKey* event) const {
  ui::AcceleratorGtk accelerator(ui::WindowsKeyCodeForGdkKeyCode(event->keyval),
                                 event->state & GDK_SHIFT_MASK,
                                 event->state & GDK_CONTROL_MASK,
                                 event->state & GDK_MOD1_MASK);
  return event_targets_.find(accelerator) != event_targets_.end();
}

void ExtensionKeybindingRegistryGtk::AddExtensionKeybinding(
    const Extension* extension) {
  // Add all the active keybindings (except page actions and browser actions,
  // which are handled elsewhere).
  ExtensionCommandService* command_service =
      ExtensionCommandServiceFactory::GetForProfile(profile_);
  const Extension::CommandMap& commands =
      command_service->GetActiveNamedCommands(extension->id());
  Extension::CommandMap::const_iterator iter = commands.begin();
  for (; iter != commands.end(); ++iter) {
    ui::AcceleratorGtk accelerator(iter->second.accelerator().key_code(),
                                   iter->second.accelerator().IsShiftDown(),
                                   iter->second.accelerator().IsCtrlDown(),
                                   iter->second.accelerator().IsAltDown());
    event_targets_[accelerator] =
        std::make_pair(extension->id(), iter->second.command_name());

    if (ShouldIgnoreCommand(iter->second.command_name()))
      continue;

    if (!accel_group_) {
      accel_group_ = gtk_accel_group_new();
      gtk_window_add_accel_group(window_, accel_group_);
    }

    gtk_accel_group_connect(
        accel_group_,
        accelerator.GetGdkKeyCode(),
        accelerator.gdk_modifier_type(),
        GtkAccelFlags(0),
        g_cclosure_new(G_CALLBACK(OnGtkAcceleratorThunk), this, NULL));
  }
}

void ExtensionKeybindingRegistryGtk::RemoveExtensionKeybinding(
    const Extension* extension) {
  EventTargets::iterator iter = event_targets_.begin();
  while (iter != event_targets_.end()) {
    if (iter->second.first != extension->id()) {
      ++iter;
      continue;  // Not the extension we asked for.
    }

    // On GTK, unlike Windows, the Event Targets contain all events.
    if (ShouldIgnoreCommand(iter->second.second)) {
      ++iter;
      continue;
    }

    gtk_accel_group_disconnect_key(accel_group_,
                                   iter->first.GetGdkKeyCode(),
                                   iter->first.gdk_modifier_type());
    EventTargets::iterator old = iter++;
    event_targets_.erase(old);
  }
}

gboolean ExtensionKeybindingRegistryGtk::OnGtkAccelerator(
    GtkAccelGroup* group,
    GObject* acceleratable,
    guint keyval,
    GdkModifierType modifier) {
  ui::AcceleratorGtk accelerator(keyval, modifier);

  ExtensionService* service = profile_->GetExtensionService();

  EventTargets::iterator it = event_targets_.find(accelerator);
  if (it == event_targets_.end()) {
    NOTREACHED();  // Shouldn't get this event for something not registered.
    return FALSE;
  }

  service->browser_event_router()->CommandExecuted(
      profile_, it->second.first, it->second.second);

  return TRUE;
}
