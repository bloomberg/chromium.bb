// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/proxy_overridden_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace {

// A map of all profiles evaluated, so we can tell if it's the initial check.
// TODO(devlin): It would be nice to coalesce all the "profiles evaluated" maps
// that are in the different bubble controllers.
base::LazyInstance<std::set<Profile*> > g_profiles_evaluated =
    LAZY_INSTANCE_INITIALIZER;

// Currently, we only show these bubbles on windows platforms. This can be
// overridden for testing purposes.
#if defined(OS_WIN)
bool g_enabled = true;
#else
bool g_enabled = false;
#endif

}  // namespace

ExtensionMessageBubbleFactory::ExtensionMessageBubbleFactory(Profile* profile)
    : profile_(profile) {
}

ExtensionMessageBubbleFactory::~ExtensionMessageBubbleFactory() {
}

scoped_ptr<extensions::ExtensionMessageBubbleController>
ExtensionMessageBubbleFactory::GetController() {
  if (!g_enabled)
    return scoped_ptr<extensions::ExtensionMessageBubbleController>();

  Profile* original_profile = profile_->GetOriginalProfile();
  std::set<Profile*>& profiles_evaluated = g_profiles_evaluated.Get();
  bool is_initial_check = profiles_evaluated.count(original_profile) == 0;
  profiles_evaluated.insert(original_profile);

  // The list of suspicious extensions takes priority over the dev mode bubble
  // and the settings API bubble, since that needs to be shown as soon as we
  // disable something. The settings API bubble is shown on first startup after
  // an extension has changed the startup pages and it is acceptable if that
  // waits until the next startup because of the suspicious extension bubble.
  // The dev mode bubble is not time sensitive like the other two so we'll catch
  // the dev mode extensions on the next startup/next window that opens. That
  // way, we're not too spammy with the bubbles.
  {
    scoped_ptr<extensions::SuspiciousExtensionBubbleController> controller(
        new extensions::SuspiciousExtensionBubbleController(profile_));
    if (controller->ShouldShow())
      return controller.Pass();
  }

  {
    // No use showing this if it's not the startup of the profile.
    if (is_initial_check) {
      scoped_ptr<extensions::SettingsApiBubbleController> controller(
          new extensions::SettingsApiBubbleController(
              profile_, extensions::BUBBLE_TYPE_STARTUP_PAGES));
      if (controller->ShouldShow())
        return controller.Pass();
    }
  }

  {
    // TODO(devlin): Move the "GetExtensionOverridingProxy" part into the
    // proxy bubble controller.
    const extensions::Extension* extension =
        extensions::GetExtensionOverridingProxy(profile_);
    if (extension) {
      scoped_ptr<extensions::ProxyOverriddenBubbleController> controller(
         new extensions::ProxyOverriddenBubbleController(profile_));
      if (controller->ShouldShow(extension->id()))
        return controller.Pass();
    }
  }

  {
    scoped_ptr<extensions::DevModeBubbleController> controller(
        new extensions::DevModeBubbleController(profile_));
    if (controller->ShouldShow())
      return controller.Pass();
  }

  return scoped_ptr<extensions::ExtensionMessageBubbleController>();
}

// static
void ExtensionMessageBubbleFactory::set_enabled_for_tests(bool enabled) {
  g_enabled = enabled;
}
