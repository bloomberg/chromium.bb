// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/frame/custom_frame_header.h"
#include "ash/public/interfaces/split_view.mojom.h"
#include "ash/shell_observer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/ash/browser_image_registrar.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window_observer.h"

class Browser;

namespace {
class HostedAppNonClientFrameViewAshTest;
}

class HostedAppButtonContainer;
class TabIconView;

namespace ash {
class DefaultFrameHeader;
class FrameCaptionButton;
class FrameCaptionButtonContainerView;
}  // namespace ash

// Provides the BrowserNonClientFrameView for Chrome OS.
class BrowserNonClientFrameViewAsh
    : public BrowserNonClientFrameView,
      public ash::CustomFrameHeader::AppearanceProvider,
      public ash::ShellObserver,
      public TabletModeClientObserver,
      public TabIconViewModel,
      public CommandObserver,
      public ash::mojom::SplitViewObserver,
      public aura::WindowObserver,
      public ImmersiveModeController::Observer {
 public:
  BrowserNonClientFrameViewAsh(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewAsh() override;

  void Init();

  ash::mojom::SplitViewObserverPtr CreateInterfacePtrForTesting();

  // BrowserNonClientFrameView:
  void OnSingleTabModeChanged() override;
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  void UpdateClientArea() override;
  void UpdateMinimumSize() override;
  int GetTabStripLeftInset() const override;
  void OnTabsMaxXChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void ActivationChanged(bool active) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // ash::CustomFrameHeader::AppearanceProvider:
  SkColor GetTitleColor() override;
  SkColor GetFrameHeaderColor(bool active) override;
  gfx::ImageSkia GetFrameHeaderImage(bool active) override;
  int GetFrameHeaderImageYInset() override;
  gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) override;
  bool IsTabletMode() const override;

  // ash::ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // TabletModeClientObserver:
  void OnTabletModeToggled(bool enabled) override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // ash::mojom::SplitViewObserver:
  void OnSplitViewStateChanged(
      ash::mojom::SplitViewState current_state) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;

  HostedAppButtonContainer* GetHostedAppButtonContainerForTesting() const;

  // Returns true if the header should be painted so that it looks the same as
  // the header used for packaged apps.
  static bool UsePackagedAppHeaderStyle(const Browser* browser);

 protected:
  // BrowserNonClientFrameView:
  AvatarButtonStyle GetAvatarButtonStyle() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           NonImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleTabletModeRelayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           AvatarDisplayOnTeleportedWindow);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           HeaderVisibilityInOverviewAndSplitview);
  FRIEND_TEST_ALL_PREFIXES(NonHomeLauncherBrowserNonClientFrameViewAshTest,
                           HeaderHeightForSnappedBrowserInSplitView);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshBackButtonTest,
                           V1BackButton);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleTabletModeOnMinimizedWindow);
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest,
                           ActiveStateOfButtonMatchesWidget);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           RestoreMinimizedBrowserUpdatesCaption);
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeControllerAshHostedAppBrowserTest,
                           FrameLayoutToggleTabletMode);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewAshTest,
                           TabletModeBrowserCaptionButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewAshTest,
                           TabletModeAppCaptionButtonVisibility);

  friend class HostedAppNonClientFrameViewAshTest;
  friend class ImmersiveModeControllerAshHostedAppBrowserTest;

  // Returns whether the caption buttons should be visible. They are hidden, for
  // example, in overview mode and tablet mode.
  bool ShouldShowCaptionButtons() const;

  // Distance between the right edge of the NonClientFrameView and the tab
  // strip.
  int GetTabStripRightInset() const;

  // Returns true if there is anything to paint. Some fullscreen windows do
  // not need their frames painted.
  bool ShouldPaint() const;

  // Helps to hide or show the header as needed when overview mode starts or
  // ends or when split view state changes.
  void OnOverviewOrSplitviewModeChanged();

  // Creates the frame header for the browser window.
  std::unique_ptr<ash::FrameHeader> CreateFrameHeader();

  // Creates views and does other setup for a hosted app.
  // TODO(estade): remove the parameter as it's unused in Mash.
  void SetUpForHostedApp(ash::DefaultFrameHeader* header);

  // Triggers the hosted app origin and icon animations, assumes the hosted
  // app UI elements exist.
  void StartHostedAppAnimation();

  // To be called after the frame's colors may have changed.
  void UpdateFrameColors();

  // View which contains the window controls.
  ash::FrameCaptionButtonContainerView* caption_button_container_ = nullptr;

  ash::FrameCaptionButton* back_button_ = nullptr;

  // For popups, the window icon.
  TabIconView* window_icon_ = nullptr;

  // Helper class for painting the header.
  std::unique_ptr<ash::FrameHeader> frame_header_;

  // Container for extra frame buttons shown for hosted app windows.
  // Owned by views hierarchy.
  HostedAppButtonContainer* hosted_app_button_container_ = nullptr;

  // A view that contains the extra views used for hosted apps
  // (|hosted_app_button_container_| and |hosted_app_origin_text_|).
  // Only used in Mash.
  views::View* hosted_app_extras_container_ = nullptr;

  // Ash's mojom::SplitViewController.
  ash::mojom::SplitViewControllerPtr split_view_controller_;

  // The binding this instance uses to implement mojom::SplitViewObserver.
  mojo::Binding<ash::mojom::SplitViewObserver> observer_binding_{this};

  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  // Indicates whether overview mode is active. Hide the header for V1 apps in
  // overview mode because a fake header is added for better UX. If also in
  // immersive mode before entering overview mode, the flag will be ignored
  // because the reveal lock will determine the show/hide header.
  bool in_overview_mode_ = false;

  // Maintains the current split view state.
  ash::mojom::SplitViewState split_view_state_ =
      ash::mojom::SplitViewState::NO_SNAP;

  // A reference to the entry in BrowserImageRegistrar for each frame
  // image. Multiple windows that share a browser theme will hold onto each ref.
  scoped_refptr<ImageRegistration> active_frame_image_registration_;
  scoped_refptr<ImageRegistration> inactive_frame_image_registration_;
  scoped_refptr<ImageRegistration> active_frame_overlay_image_registration_;
  scoped_refptr<ImageRegistration> inactive_frame_overlay_image_registration_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
