// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_widget_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"

views::NativeWidget* CreateNativeWidget(
    NativeWidgetType type,
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // While the majority of the time, context wasn't plumbed through due to the
  // existence of a global WindowParentingClient, if this window is toplevel,
  // it's possible that there is no contextual state that we can use.
  gfx::NativeWindow parent_or_context =
      params->parent ? params->parent : params->context;
  // Set the profile key based on the profile of |parent_or_context|
  // so that the widget will be styled with the apropriate
  // NativeTheme.  For browser windows, BrowserView will reset the
  // profile key to profile of the corresponding Browser.
  Profile* profile = nullptr;
  if (parent_or_context) {
    profile = reinterpret_cast<Profile*>(
        parent_or_context->GetNativeWindowProperty(Profile::kProfileKey));
  }
  // Use the original profile because |window| may outlive the profile
  // of the context window.  This can happen with incognito profiles.
  // However, the original profile will stick around until shutdown.
  if (profile)
    profile = profile->GetOriginalProfile();
  if (type == NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA ||
      (!params->parent && !params->context && !params->child)) {
    views::DesktopNativeWidgetAura* desktop_native_widget =
        new views::DesktopNativeWidgetAura(delegate);
    desktop_native_widget->SetNativeWindowProperty(Profile::kProfileKey,
                                                   profile);
    return desktop_native_widget;
  }
  views::NativeWidgetAura* native_widget_aura =
      new views::NativeWidgetAura(delegate);
  native_widget_aura->SetNativeWindowProperty(Profile::kProfileKey, profile);
  return native_widget_aura;
}
