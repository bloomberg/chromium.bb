// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/mus_property_mirror_ash.h"

#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace {

template <typename T>
void MirrorOwnedProperty(aura::Window* window,
                         aura::Window* root_window,
                         const aura::WindowProperty<T*>* key) {
  T* value = window->GetProperty(key);
  if (!value)
    root_window->ClearProperty(key);
  else
    root_window->SetProperty(key, new T(*value));
}

}  // namespace

namespace ash {

MusPropertyMirrorAsh::MusPropertyMirrorAsh() = default;
MusPropertyMirrorAsh::~MusPropertyMirrorAsh() = default;

void MusPropertyMirrorAsh::MirrorPropertyFromWidgetWindowToRootWindow(
    aura::Window* window,
    aura::Window* root_window,
    const void* key) {
  if (key == aura::client::kAppType) {
    root_window->SetProperty(aura::client::kAppType,
                             window->GetProperty(aura::client::kAppType));
  } else if (key == kBlockedForAssistantSnapshotKey) {
    root_window->SetProperty(
        kBlockedForAssistantSnapshotKey,
        window->GetProperty(kBlockedForAssistantSnapshotKey));
  } else if (key == kPanelAttachedKey) {
    bool value = window->GetProperty(kPanelAttachedKey);
    root_window->SetProperty(kPanelAttachedKey, value);
  } else if (key == kShelfItemTypeKey) {
    int32_t value = window->GetProperty(kShelfItemTypeKey);
    root_window->SetProperty(kShelfItemTypeKey, value);
  } else if (key == kWindowStateTypeKey) {
    ash::mojom::WindowStateType value =
        window->GetProperty(kWindowStateTypeKey);
    root_window->SetProperty(kWindowStateTypeKey, value);
  } else if (key == kWindowPinTypeKey) {
    ash::mojom::WindowPinType value = window->GetProperty(kWindowPinTypeKey);
    root_window->SetProperty(kWindowPinTypeKey, value);
  } else if (key == aura::client::kAppIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kAppIconKey);
  } else if (key == kRestoreBoundsOverrideKey) {
    MirrorOwnedProperty(window, root_window, kRestoreBoundsOverrideKey);
  } else if (key == kRestoreWindowStateTypeOverrideKey) {
    ash::mojom::WindowStateType value =
        window->GetProperty(kRestoreWindowStateTypeOverrideKey);
    root_window->SetProperty(kRestoreWindowStateTypeOverrideKey, value);
  } else if (key == kShelfIDKey) {
    MirrorOwnedProperty(window, root_window, kShelfIDKey);
  } else if (key == aura::client::kDrawAttentionKey) {
    bool value = window->GetProperty(aura::client::kDrawAttentionKey);
    root_window->SetProperty(aura::client::kDrawAttentionKey, value);
  } else if (key == aura::client::kMinimumSize) {
    MirrorOwnedProperty(window, root_window, aura::client::kMinimumSize);
  } else if (key == aura::client::kTitleKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kTitleKey);
  } else if (key == aura::client::kTitleShownKey) {
    root_window->SetProperty(aura::client::kTitleShownKey,
                             window->GetProperty(aura::client::kTitleShownKey));
  } else if (key == aura::client::kTopViewInset) {
    root_window->SetProperty(aura::client::kTopViewInset,
                             window->GetProperty(aura::client::kTopViewInset));

  } else if (key == aura::client::kWindowIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kWindowIconKey);
  } else if (key == kFrameBackButtonStateKey) {
    root_window->SetProperty(kFrameBackButtonStateKey,
                             window->GetProperty(kFrameBackButtonStateKey));
  } else if (key == kFrameActiveColorKey) {
    root_window->SetProperty(kFrameActiveColorKey,
                             window->GetProperty(kFrameActiveColorKey));
  } else if (key == kFrameInactiveColorKey) {
    root_window->SetProperty(kFrameInactiveColorKey,
                             window->GetProperty(kFrameInactiveColorKey));
  } else if (key == kFrameImageActiveKey) {
    MirrorOwnedProperty(window, root_window, kFrameImageActiveKey);
  } else if (key == kFrameImageInactiveKey) {
    MirrorOwnedProperty(window, root_window, kFrameImageInactiveKey);
  } else if (key == kFrameImageOverlayActiveKey) {
    MirrorOwnedProperty(window, root_window, kFrameImageOverlayActiveKey);
  } else if (key == kFrameImageOverlayInactiveKey) {
    MirrorOwnedProperty(window, root_window, kFrameImageOverlayInactiveKey);
  } else if (key == kFrameImageYInsetKey) {
    root_window->SetProperty(kFrameImageYInsetKey,
                             window->GetProperty(kFrameImageYInsetKey));
  } else if (key == kFrameIsThemedByHostedAppKey) {
    root_window->SetProperty(kFrameIsThemedByHostedAppKey,
                             window->GetProperty(kFrameIsThemedByHostedAppKey));
  } else if (key == kFrameTextColorKey) {
    root_window->SetProperty(kFrameTextColorKey,
                             window->GetProperty(kFrameTextColorKey));
  } else if (key == kHideCaptionButtonsInTabletModeKey) {
    root_window->SetProperty(
        kHideCaptionButtonsInTabletModeKey,
        window->GetProperty(kHideCaptionButtonsInTabletModeKey));
  } else if (key == kImmersiveImpliedByFullscreen) {
    root_window->SetProperty(
        kImmersiveImpliedByFullscreen,
        window->GetProperty(kImmersiveImpliedByFullscreen));
  } else if (key == kImmersiveIsActive) {
    root_window->SetProperty(kImmersiveIsActive,
                             window->GetProperty(kImmersiveIsActive));
  } else if (key == kImmersiveTopContainerBoundsInScreen) {
    MirrorOwnedProperty(window, root_window,
                        kImmersiveTopContainerBoundsInScreen);
  }
}

}  // namespace ash
