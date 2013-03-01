// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/panels/test_panel_active_state_observer.h"
#include "chrome/browser/ui/panels/test_panel_mouse_watcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/web_contents_tester.h"
#include "sync/api/string_ordinal.h"

#if defined(OS_LINUX)
#include "chrome/browser/ui/browser_window.h"
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#endif

using content::WebContentsTester;
using extensions::Extension;

namespace {

const gfx::Rect kTestingPrimaryScreenArea = gfx::Rect(0, 0, 800, 600);
const gfx::Rect kTestingWorkArea = gfx::Rect(0, 0, 800, 580);

struct MockDesktopBar {
  bool auto_hiding_enabled;
  DisplaySettingsProvider::DesktopBarVisibility visibility;
  int thickness;
};

class MockDisplaySettingsProviderImpl :
    public BasePanelBrowserTest::MockDisplaySettingsProvider {
 public:
  explicit MockDisplaySettingsProviderImpl(PanelManager* panel_manager);
  virtual ~MockDisplaySettingsProviderImpl() { }

  // Overridden from DisplaySettingsProvider:
  virtual gfx::Rect GetPrimaryScreenArea() const OVERRIDE;
  virtual gfx::Rect GetWorkArea() const OVERRIDE;
  virtual bool IsAutoHidingDesktopBarEnabled(
      DesktopBarAlignment alignment) OVERRIDE;
  virtual int GetDesktopBarThickness(
      DesktopBarAlignment alignment) const OVERRIDE;
  virtual DesktopBarVisibility GetDesktopBarVisibility(
      DesktopBarAlignment alignment) const OVERRIDE;

  // Overridden from MockDisplaySettingsProvider:
  virtual void SetPrimaryScreenArea(
      const gfx::Rect& primary_screen_area) OVERRIDE;
  virtual void SetWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual void EnableAutoHidingDesktopBar(DesktopBarAlignment alignment,
                                          bool enabled,
                                          int thickness) OVERRIDE;
  virtual void SetDesktopBarVisibility(
      DesktopBarAlignment alignment, DesktopBarVisibility visibility) OVERRIDE;
  virtual void SetDesktopBarThickness(DesktopBarAlignment alignment,
                                      int thickness) OVERRIDE;

 private:
  gfx::Rect testing_primary_screen_area_;
  gfx::Rect testing_work_area_;
  MockDesktopBar mock_desktop_bars[3];

  DISALLOW_COPY_AND_ASSIGN(MockDisplaySettingsProviderImpl);
};


MockDisplaySettingsProviderImpl::MockDisplaySettingsProviderImpl(
    PanelManager* panel_manager) {
  DisplaySettingsProvider* old_provider =
      panel_manager->display_settings_provider();

  ObserverListBase<DisplaySettingsProvider::DisplayAreaObserver>::Iterator
      display_area_observers_iter(old_provider->display_area_observers());
  AddDisplayAreaObserver(display_area_observers_iter.GetNext());
  DCHECK(!display_area_observers_iter.GetNext());

  ObserverListBase<DisplaySettingsProvider::DesktopBarObserver>::Iterator
      desktop_bar_observer_iter(old_provider->desktop_bar_observers());
  AddDesktopBarObserver(desktop_bar_observer_iter.GetNext());
  DCHECK(!desktop_bar_observer_iter.GetNext());

  ObserverListBase<DisplaySettingsProvider::FullScreenObserver>::Iterator
      full_screen_observer_iter(old_provider->full_screen_observers());
  AddFullScreenObserver(full_screen_observer_iter.GetNext());
  DCHECK(!full_screen_observer_iter.GetNext());

  panel_manager->set_display_settings_provider(this);

  memset(mock_desktop_bars, 0, sizeof(mock_desktop_bars));
}

gfx::Rect MockDisplaySettingsProviderImpl::GetPrimaryScreenArea() const {
  return testing_primary_screen_area_;
}

gfx::Rect MockDisplaySettingsProviderImpl::GetWorkArea() const {
  return testing_work_area_;
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

void MockDisplaySettingsProviderImpl::EnableAutoHidingDesktopBar(
    DesktopBarAlignment alignment, bool enabled, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  bar->auto_hiding_enabled = enabled;
  bar->thickness = thickness;
  OnAutoHidingDesktopBarChanged();
}

void MockDisplaySettingsProviderImpl::SetPrimaryScreenArea(
    const gfx::Rect& primary_screen_area) {
  testing_primary_screen_area_ = primary_screen_area;
}

void MockDisplaySettingsProviderImpl::SetWorkArea(const gfx::Rect& work_area) {
  testing_work_area_ = work_area;
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
  OnAutoHidingDesktopBarChanged();
}

void MockDisplaySettingsProviderImpl::SetDesktopBarThickness(
    DesktopBarAlignment alignment, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  if (!bar->auto_hiding_enabled)
    return;
  if (thickness == bar->thickness)
    return;
  bar->thickness = thickness;
  OnAutoHidingDesktopBarChanged();
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

bool BasePanelBrowserTest::SkipTestIfIceWM() {
#if defined(OS_LINUX)
  return ui::GuessWindowManager() == ui::WM_ICE_WM;
#else
  return false;
#endif
}

bool BasePanelBrowserTest::SkipTestIfCompizWM() {
#if defined(OS_LINUX)
  return ui::GuessWindowManager() == ui::WM_COMPIZ;
#else
  return false;
#endif
}

void BasePanelBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnablePanels);
  command_line->AppendSwitch(switches::kEnablePanelStacking);
}

void BasePanelBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  // Setup the work area and desktop bar so that we have consistent testing
  // environment for all panel related tests.
  PanelManager* panel_manager = PanelManager::GetInstance();
  if (mock_display_settings_enabled_) {
    mock_display_settings_provider_ =
        new MockDisplaySettingsProviderImpl(panel_manager);
    SetTestingAreas(kTestingPrimaryScreenArea, kTestingWorkArea);
  }

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
    MessageLoopForUI::current()->RunUntilIdle();

#if defined(OS_LINUX)
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
  panel->manager()->MovePanelToCollection(panel,
                                          stack,
                                          PanelCollection::DEFAULT_POSITION);
  EXPECT_EQ(PanelCollection::STACKED, panel->collection()->type());
  WaitForBoundsAnimationFinished(panel);
  return panel;
}

// static
NativePanelTesting* BasePanelBrowserTest::CreateNativePanelTesting(
    Panel* panel) {
  return panel->native_panel()->CreateNativePanelTesting();
}

scoped_refptr<Extension> BasePanelBrowserTest::CreateExtension(
    const base::FilePath::StringType& path,
    extensions::Manifest::Location location,
    const DictionaryValue& extra_value) {
#if defined(OS_WIN)
  base::FilePath full_path(FILE_PATH_LITERAL("c:\\"));
#else
  base::FilePath full_path(FILE_PATH_LITERAL("/"));
#endif
  full_path = full_path.Append(path);

  scoped_ptr<DictionaryValue> input_value(extra_value.DeepCopy());
  input_value->SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  input_value->SetString(extension_manifest_keys::kName, "Sample Extension");

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      full_path,  location, *input_value, Extension::NO_FLAGS, &error);
  EXPECT_TRUE(extension.get());
  EXPECT_STREQ("", error.c_str());
  browser()->profile()->GetExtensionService()->
      OnExtensionInstalled(extension.get(), syncer::StringOrdinal(),
                           false /* no requirement errors */,
                           false /* don't wait for idle */);
  return extension;
}

void BasePanelBrowserTest::SetTestingAreas(const gfx::Rect& primary_screen_area,
                                           const gfx::Rect& work_area) {
  DCHECK(primary_screen_area.Contains(work_area));
  mock_display_settings_provider_->SetPrimaryScreenArea(primary_screen_area);
  mock_display_settings_provider_->SetWorkArea(
      work_area.IsEmpty() ? primary_screen_area : work_area);
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
