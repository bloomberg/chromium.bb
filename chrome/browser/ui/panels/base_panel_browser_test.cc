// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/panels/test_panel_active_state_observer.h"
#include "chrome/browser/ui/panels/test_panel_mouse_watcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/manifest_constants.h"
#include "sync/api/string_ordinal.h"

#if defined(OS_LINUX)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#endif

using content::WebContentsTester;
using extensions::Extension;

namespace {

const gfx::Rect kTestingPrimaryDisplayArea = gfx::Rect(0, 0, 800, 600);
const gfx::Rect kTestingPrimaryWorkArea = gfx::Rect(0, 0, 800, 580);

struct MockDesktopBar {
  bool auto_hiding_enabled;
  DisplaySettingsProvider::DesktopBarVisibility visibility;
  int thickness;
};

class MockDisplaySettingsProviderImpl :
    public BasePanelBrowserTest::MockDisplaySettingsProvider {
 public:
  explicit MockDisplaySettingsProviderImpl();
  virtual ~MockDisplaySettingsProviderImpl() { }

  // Overridden from DisplaySettingsProvider:
  virtual gfx::Rect GetPrimaryDisplayArea() const OVERRIDE;
  virtual gfx::Rect GetPrimaryWorkArea() const OVERRIDE;
  virtual gfx::Rect GetDisplayAreaMatching(
      const gfx::Rect& bounds) const OVERRIDE;
  virtual gfx::Rect GetWorkAreaMatching(
      const gfx::Rect& bounds) const OVERRIDE;
  virtual bool IsAutoHidingDesktopBarEnabled(
      DesktopBarAlignment alignment) OVERRIDE;
  virtual int GetDesktopBarThickness(
      DesktopBarAlignment alignment) const OVERRIDE;
  virtual DesktopBarVisibility GetDesktopBarVisibility(
      DesktopBarAlignment alignment) const OVERRIDE;
  virtual bool IsFullScreen() OVERRIDE;

  // Overridden from MockDisplaySettingsProvider:
  virtual void SetPrimaryDisplay(
      const gfx::Rect& display_area, const gfx::Rect& work_area) OVERRIDE;
  virtual void SetSecondaryDisplay(
      const gfx::Rect& display_area, const gfx::Rect& work_area) OVERRIDE;
  virtual void EnableAutoHidingDesktopBar(DesktopBarAlignment alignment,
                                          bool enabled,
                                          int thickness) OVERRIDE;
  virtual void SetDesktopBarVisibility(
      DesktopBarAlignment alignment, DesktopBarVisibility visibility) OVERRIDE;
  virtual void SetDesktopBarThickness(DesktopBarAlignment alignment,
                                      int thickness) OVERRIDE;
  virtual void EnableFullScreenMode(bool enabled) OVERRIDE;

 private:
  gfx::Rect primary_display_area_;
  gfx::Rect primary_work_area_;
  gfx::Rect secondary_display_area_;
  gfx::Rect secondary_work_area_;
  MockDesktopBar mock_desktop_bars[3];
  bool full_screen_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MockDisplaySettingsProviderImpl);
};


MockDisplaySettingsProviderImpl::MockDisplaySettingsProviderImpl()
    : full_screen_enabled_(false) {
  memset(mock_desktop_bars, 0, sizeof(mock_desktop_bars));
}

gfx::Rect MockDisplaySettingsProviderImpl::GetPrimaryDisplayArea() const {
  return primary_display_area_;
}

gfx::Rect MockDisplaySettingsProviderImpl::GetPrimaryWorkArea() const {
  return primary_work_area_;
}

gfx::Rect MockDisplaySettingsProviderImpl::GetDisplayAreaMatching(
    const gfx::Rect& bounds) const {
  if (secondary_display_area_.IsEmpty())
    return primary_display_area_;

  gfx::Rect primary_intersection =
      gfx::IntersectRects(bounds, primary_display_area_);
  int primary_intersection_size =
      primary_intersection.width() * primary_intersection.height();

  gfx::Rect secondary_intersection =
      gfx::IntersectRects(bounds, secondary_display_area_);
  int secondary_intersection_size =
      secondary_intersection.width() * secondary_intersection.height();

  return primary_intersection_size >= secondary_intersection_size ?
      primary_display_area_ : secondary_display_area_;
}

gfx::Rect MockDisplaySettingsProviderImpl::GetWorkAreaMatching(
    const gfx::Rect& bounds) const {
  if (secondary_work_area_.IsEmpty())
    return primary_work_area_;

  gfx::Rect primary_intersection =
      gfx::IntersectRects(bounds, primary_work_area_);
  int primary_intersection_size =
      primary_intersection.width() * primary_intersection.height();

  gfx::Rect secondary_intersection =
      gfx::IntersectRects(bounds, secondary_work_area_);
  int secondary_intersection_size =
      secondary_intersection.width() * secondary_intersection.height();

  return primary_intersection_size >= secondary_intersection_size ?
      primary_work_area_ : secondary_work_area_;
}

bool MockDisplaySettingsProviderImpl::IsAutoHidingDesktopBarEnabled(
    DesktopBarAlignment alignment) {
  return mock_desktop_bars[static_cast<int>(alignment)].auto_hiding_enabled;
}

int MockDisplaySettingsProviderImpl::GetDesktopBarThickness(
    DesktopBarAlignment alignment) const {
  return mock_desktop_bars[static_cast<int>(alignment)].thickness;
}

DisplaySettingsProvider::DesktopBarVisibility
MockDisplaySettingsProviderImpl::GetDesktopBarVisibility(
    DesktopBarAlignment alignment) const {
  return mock_desktop_bars[static_cast<int>(alignment)].visibility;
}

bool MockDisplaySettingsProviderImpl::IsFullScreen() {
  return full_screen_enabled_;
}

void MockDisplaySettingsProviderImpl::EnableAutoHidingDesktopBar(
    DesktopBarAlignment alignment, bool enabled, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  bar->auto_hiding_enabled = enabled;
  bar->thickness = thickness;
}

void MockDisplaySettingsProviderImpl::SetPrimaryDisplay(
    const gfx::Rect& display_area, const gfx::Rect& work_area) {
  DCHECK(display_area.Contains(work_area));
  primary_display_area_ = display_area;
  primary_work_area_ = work_area;
  OnDisplaySettingsChanged();
}

void MockDisplaySettingsProviderImpl::SetSecondaryDisplay(
    const gfx::Rect& display_area, const gfx::Rect& work_area) {
  DCHECK(display_area.Contains(work_area));
  secondary_display_area_ = display_area;
  secondary_work_area_ = work_area;
  OnDisplaySettingsChanged();
}

void MockDisplaySettingsProviderImpl::SetDesktopBarVisibility(
    DesktopBarAlignment alignment, DesktopBarVisibility visibility) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  if (!bar->auto_hiding_enabled)
    return;
  if (visibility == bar->visibility)
    return;
  bar->visibility = visibility;
  FOR_EACH_OBSERVER(
      DesktopBarObserver,
      desktop_bar_observers(),
      OnAutoHidingDesktopBarVisibilityChanged(alignment, visibility));
}

void MockDisplaySettingsProviderImpl::SetDesktopBarThickness(
    DesktopBarAlignment alignment, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  if (!bar->auto_hiding_enabled)
    return;
  if (thickness == bar->thickness)
    return;
  bar->thickness = thickness;
  FOR_EACH_OBSERVER(
      DesktopBarObserver,
      desktop_bar_observers(),
      OnAutoHidingDesktopBarThicknessChanged(alignment, thickness));
}

void MockDisplaySettingsProviderImpl::EnableFullScreenMode(bool enabled) {
  full_screen_enabled_ = enabled;
  CheckFullScreenMode(PERFORM_FULLSCREEN_CHECK);
}

}  // namespace

const base::FilePath::CharType* BasePanelBrowserTest::kTestDir =
    FILE_PATH_LITERAL("panels");

BasePanelBrowserTest::BasePanelBrowserTest()
    : InProcessBrowserTest(),
      mock_display_settings_enabled_(true) {
}

BasePanelBrowserTest::~BasePanelBrowserTest() {
}

void BasePanelBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnablePanels);
}

void BasePanelBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  // Setup the work area and desktop bar so that we have consistent testing
  // environment for all panel related tests.
  if (mock_display_settings_enabled_) {
    mock_display_settings_provider_ = new MockDisplaySettingsProviderImpl();
    mock_display_settings_provider_->SetPrimaryDisplay(
        kTestingPrimaryDisplayArea, kTestingPrimaryWorkArea);
    PanelManager::SetDisplaySettingsProviderForTesting(
        mock_display_settings_provider_);
  }

  PanelManager* panel_manager = PanelManager::GetInstance();
  panel_manager->enable_auto_sizing(false);

  PanelManager::shorten_time_intervals_for_testing();

  // Simulate the mouse movement so that tests are not affected by actual mouse
  // events.
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  panel_manager->SetMouseWatcherForTesting(mouse_watcher);

  // This is needed so the subsequently created panels can be activated.
  // On a Mac, it transforms background-only test process into foreground one.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
}

void BasePanelBrowserTest::WaitForPanelActiveState(
    Panel* panel, ActiveState expected_state) {
  DCHECK(expected_state == SHOW_AS_ACTIVE ||
         expected_state == SHOW_AS_INACTIVE);

#if defined(OS_MACOSX)
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  ASSERT_TRUE(panel_testing->EnsureApplicationRunOnForeground()) <<
      "Failed to bring application to foreground. Bail out.";
#endif

  PanelActiveStateObserver signal(panel, expected_state == SHOW_AS_ACTIVE);
  signal.Wait();
}

void BasePanelBrowserTest::WaitForWindowSizeAvailable(Panel* panel) {
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_WINDOW_SIZE_KNOWN,
      content::Source<Panel>(panel));
  if (panel_testing->IsWindowSizeKnown())
    return;
  signal.Wait();
  EXPECT_TRUE(panel_testing->IsWindowSizeKnown());
}

void BasePanelBrowserTest::WaitForBoundsAnimationFinished(Panel* panel) {
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  // Sometimes there are several animations in sequence due to content
  // auto resizing. Wait for all animations to finish.
  while (panel_testing->IsAnimatingBounds()) {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
        content::Source<Panel>(panel));
    if (!panel_testing->IsAnimatingBounds())
      return;
    signal.Wait();
  }
}

BasePanelBrowserTest::CreatePanelParams::CreatePanelParams(
    const std::string& name,
    const gfx::Rect& bounds,
    ActiveState show_flag)
    : name(name),
      bounds(bounds),
      show_flag(show_flag),
      wait_for_fully_created(true),
      expected_active_state(show_flag),
      create_mode(PanelManager::CREATE_AS_DOCKED),
      profile(NULL) {
}

Panel* BasePanelBrowserTest::CreatePanelWithParams(
    const CreatePanelParams& params) {
#if defined(OS_MACOSX)
  // Opening panels on a Mac causes NSWindowController of the Panel window
  // to be autoreleased. We need a pool drained after it's done so the test
  // can close correctly. The NSWindowController of the Panel window controls
  // lifetime of the Panel object so we want to release it as soon as
  // possible. In real Chrome, this is done by message pump.
  // On non-Mac platform, this is an empty class.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  PanelManager* manager = PanelManager::GetInstance();
  Panel* panel = manager->CreatePanel(
      params.name,
      params.profile ? params.profile : browser()->profile(),
      params.url,
      params.bounds,
      params.create_mode);

  if (!params.url.is_empty())
    observer.Wait();

  if (!manager->auto_sizing_enabled() ||
      params.bounds.width() || params.bounds.height()) {
    EXPECT_FALSE(panel->auto_resizable());
  } else {
    EXPECT_TRUE(panel->auto_resizable());
  }

  if (params.show_flag == SHOW_AS_ACTIVE) {
    panel->Show();
  } else {
    panel->ShowInactive();
  }

  if (params.wait_for_fully_created) {
    base::MessageLoopForUI::current()->RunUntilIdle();

#if defined(OS_LINUX) && defined(USE_X11)
    // On bots, we might have a simple window manager which always activates new
    // windows, and can't always deactivate them. Re-activate the main tabbed
    // browser to "deactivate" the newly created panel.
    if (params.expected_active_state == SHOW_AS_INACTIVE &&
        ui::GuessWindowManager() == ui::WM_ICE_WM) {
      // Wait for new panel to become active before deactivating to ensure
      // the activated notification is consumed before we wait for the panel
      // to become inactive.
      WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
      browser()->window()->Activate();
    }
#endif
    // More waiting, because gaining or losing focus may require inter-process
    // asynchronous communication, and it is not enough to just run the local
    // message loop to make sure this activity has completed.
    WaitForPanelActiveState(panel, params.expected_active_state);

    // On Linux, window size is not available right away and we should wait
    // before moving forward with the test.
    WaitForWindowSizeAvailable(panel);

    // Wait for the bounds animations on creation to finish.
    WaitForBoundsAnimationFinished(panel);
  }

  return panel;
}

Panel* BasePanelBrowserTest::CreatePanelWithBounds(
    const std::string& panel_name, const gfx::Rect& bounds) {
  CreatePanelParams params(panel_name, bounds, SHOW_AS_ACTIVE);
  return CreatePanelWithParams(params);
}

Panel* BasePanelBrowserTest::CreatePanel(const std::string& panel_name) {
  CreatePanelParams params(panel_name, gfx::Rect(), SHOW_AS_ACTIVE);
  return CreatePanelWithParams(params);
}

Panel* BasePanelBrowserTest::CreateDockedPanel(const std::string& name,
                                               const gfx::Rect& bounds) {
  Panel* panel = CreatePanelWithBounds(name, bounds);
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  return panel;
}

Panel* BasePanelBrowserTest::CreateDetachedPanel(const std::string& name,
                                                 const gfx::Rect& bounds) {
  Panel* panel = CreatePanelWithBounds(name, bounds);
  PanelManager* panel_manager = panel->manager();
  panel_manager->MovePanelToCollection(panel,
                                       panel_manager->detached_collection(),
                                       PanelCollection::DEFAULT_POSITION);
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  // The panel is first created as docked panel, which ignores the specified
  // origin in |bounds|. We need to reposition the panel after it becomes
  // detached.
  panel->SetPanelBounds(bounds);
  WaitForBoundsAnimationFinished(panel);
  return panel;
}

Panel* BasePanelBrowserTest::CreateStackedPanel(const std::string& name,
                                                const gfx::Rect& bounds,
                                                StackedPanelCollection* stack) {
  Panel* panel = CreateDetachedPanel(name, bounds);
  panel->manager()->MovePanelToCollection(
      panel,
      stack,
      static_cast<PanelCollection::PositioningMask>(
          PanelCollection::DEFAULT_POSITION |
          PanelCollection::COLLAPSE_TO_FIT));
  EXPECT_EQ(PanelCollection::STACKED, panel->collection()->type());
  WaitForBoundsAnimationFinished(panel);
  return panel;
}

Panel* BasePanelBrowserTest::CreateInactivePanel(const std::string& name) {
  // Create an active panel first, instead of inactive panel. This is because
  // certain window managers on Linux, like icewm, will always activate the
  // new window.
  Panel* panel = CreatePanel(name);

  DeactivatePanel(panel);
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  return panel;
}

Panel* BasePanelBrowserTest::CreateInactiveDockedPanel(
    const std::string& name, const gfx::Rect& bounds) {
  // Create an active panel first, instead of inactive panel. This is because
  // certain window managers on Linux, like icewm, will always activate the
  // new window.
  Panel* panel = CreateDockedPanel(name, bounds);

  DeactivatePanel(panel);
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  return panel;
}

Panel* BasePanelBrowserTest::CreateInactiveDetachedPanel(
    const std::string& name, const gfx::Rect& bounds) {
  // Create an active panel first, instead of inactive panel. This is because
  // certain window managers on Linux, like icewm, will always activate the
  // new window.
  Panel* panel = CreateDetachedPanel(name, bounds);

  DeactivatePanel(panel);
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  return panel;
}

void BasePanelBrowserTest::ActivatePanel(Panel* panel) {
  // For certain window managers on Linux, the window activation/deactivation
  // signals might not be sent. To work around this, we explicitly deactivate
  // all other panels first.
#if defined(OS_LINUX)
  std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
  for (std::vector<Panel*>::const_iterator iter = panels.begin();
       iter != panels.end(); ++iter) {
    Panel* current_panel = *iter;
    if (panel != current_panel)
      current_panel->Deactivate();
  }
#endif

  panel->Activate();
}

void BasePanelBrowserTest::DeactivatePanel(Panel* panel) {
#if defined(OS_LINUX)
  // For certain window managers on Linux, like icewm, panel activation and
  // deactivation notification might not get tiggered when non-panel window is
  // activated or deactivated. So we deactivate the panel directly.
  panel->Deactivate();
#else
  // Make the panel lose focus by activating the browser window. This is
  // because:
  // 1) On Windows, deactivating the panel window might cause the application
  //    to lose the foreground status. When this occurs, trying to activate
  //    the panel window again will not be allowed by the system.
  // 2) On MacOS, deactivating a window is not supported by Cocoa.
  browser()->window()->Activate();
#endif
}

// static
NativePanelTesting* BasePanelBrowserTest::CreateNativePanelTesting(
    Panel* panel) {
  return panel->native_panel()->CreateNativePanelTesting();
}

scoped_refptr<Extension> BasePanelBrowserTest::CreateExtension(
    const base::FilePath::StringType& path,
    extensions::Manifest::Location location,
    const base::DictionaryValue& extra_value) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());
  base::FilePath full_path = extension_prefs->install_directory().Append(path);

  scoped_ptr<base::DictionaryValue> input_value(extra_value.DeepCopy());
  input_value->SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  input_value->SetString(extensions::manifest_keys::kName, "Sample Extension");

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      full_path,  location, *input_value, Extension::NO_FLAGS, &error);
  EXPECT_TRUE(extension.get());
  EXPECT_STREQ("", error.c_str());
  extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->OnExtensionInstalled(
          extension.get(),
          syncer::StringOrdinal(),
          extensions::kInstallFlagInstallImmediately);
  return extension;
}

void BasePanelBrowserTest::CloseWindowAndWait(Panel* panel) {
  // Closing a panel may involve several async tasks. Need to use
  // message pump and wait for the notification.
  PanelManager* manager = PanelManager::GetInstance();
  int panel_count = manager->num_panels();
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel));
  panel->Close();
  signal.Wait();
  // Now we have one less panel.
  EXPECT_EQ(panel_count - 1, manager->num_panels());

#if defined(OS_MACOSX)
  // Mac window controllers may be autoreleased, and in the non-test
  // environment, may actually depend on the autorelease pool being recycled
  // with the run loop in order to perform important work. Replicate this in
  // the test environment.
  AutoreleasePool()->Recycle();

  // Make sure that everything has a chance to run.
  chrome::testing::NSRunLoopRunAllPending();
#endif  // OS_MACOSX
}

void BasePanelBrowserTest::MoveMouseAndWaitForExpansionStateChange(
    Panel* panel,
    const gfx::Point& position) {
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
      content::Source<Panel>(panel));
  MoveMouse(position);
  signal.Wait();
}

void BasePanelBrowserTest::MoveMouse(const gfx::Point& position) {
  PanelManager::GetInstance()->mouse_watcher()->NotifyMouseMovement(position);
}

std::string BasePanelBrowserTest::MakePanelName(int index) {
  std::string panel_name("Panel");
  return panel_name + base::IntToString(index);
}

bool BasePanelBrowserTest::WmSupportWindowActivation() {
  return true;
}
