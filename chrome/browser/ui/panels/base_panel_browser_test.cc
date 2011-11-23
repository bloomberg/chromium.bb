// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"

#include "chrome/browser/ui/browser_list.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#endif

namespace {

const int kTestingWorkAreaWidth = 800;
const int kTestingWorkAreaHeight = 600;
const int kDefaultAutoHidingDesktopBarThickness = 40;

struct MockDesktopBar {
  bool auto_hiding_enabled;
  AutoHidingDesktopBar::Visibility visibility;
  int thickness;
};

class MockAutoHidingDesktopBarImpl :
    public BasePanelBrowserTest::MockAutoHidingDesktopBar {
 public:
  explicit MockAutoHidingDesktopBarImpl(Observer* observer);
  virtual ~MockAutoHidingDesktopBarImpl() { }

  // Overridden from AutoHidingDesktopBar:
  virtual void UpdateWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual bool IsEnabled(Alignment alignment) OVERRIDE;
  virtual int GetThickness(Alignment alignment) const OVERRIDE;
  virtual Visibility GetVisibility(Alignment alignment) const OVERRIDE;

  // Overridden from MockAutoHidingDesktopBar:
  virtual void EnableAutoHiding(Alignment alignment,
                                bool enabled,
                                int thickness) OVERRIDE;
  virtual void SetVisibility(Alignment alignment,
                             Visibility visibility) OVERRIDE;
  virtual void SetThickness(Alignment alignment, int thickness) OVERRIDE;

  void set_observer(Observer* observer) { observer_ = observer; }

 private:
  void NotifyVisibilityChange(Alignment alignment, Visibility visibility);
  void NotifyThicknessChange();

  Observer* observer_;
  MockDesktopBar mock_desktop_bars[3];
  base::WeakPtrFactory<MockAutoHidingDesktopBarImpl> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockAutoHidingDesktopBarImpl);
};


MockAutoHidingDesktopBarImpl::MockAutoHidingDesktopBarImpl(
    AutoHidingDesktopBar::Observer* observer)
    : observer_(observer), method_factory_(this) {
  memset(mock_desktop_bars, 0, sizeof(mock_desktop_bars));
}

void MockAutoHidingDesktopBarImpl::UpdateWorkArea(
    const gfx::Rect& work_area) {
}

bool MockAutoHidingDesktopBarImpl::IsEnabled(
    AutoHidingDesktopBar::Alignment alignment) {
  return mock_desktop_bars[static_cast<int>(alignment)].auto_hiding_enabled;
}

int MockAutoHidingDesktopBarImpl::GetThickness(
    AutoHidingDesktopBar::Alignment alignment) const {
  return mock_desktop_bars[static_cast<int>(alignment)].thickness;
}

AutoHidingDesktopBar::Visibility
MockAutoHidingDesktopBarImpl::GetVisibility(
    AutoHidingDesktopBar::Alignment alignment) const {
  return mock_desktop_bars[static_cast<int>(alignment)].visibility;
}

void MockAutoHidingDesktopBarImpl::EnableAutoHiding(
    AutoHidingDesktopBar::Alignment alignment, bool enabled, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  bar->auto_hiding_enabled = enabled;
  bar->thickness = thickness;
  observer_->OnAutoHidingDesktopBarThicknessChanged();
}

void MockAutoHidingDesktopBarImpl::SetVisibility(
    AutoHidingDesktopBar::Alignment alignment,
    AutoHidingDesktopBar::Visibility visibility) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  if (!bar->auto_hiding_enabled)
    return;
  if (visibility == bar->visibility)
    return;
  bar->visibility = visibility;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockAutoHidingDesktopBarImpl::NotifyVisibilityChange,
                 method_factory_.GetWeakPtr(),
                 alignment,
                 visibility));
}

void MockAutoHidingDesktopBarImpl::SetThickness(
    AutoHidingDesktopBar::Alignment alignment, int thickness) {
  MockDesktopBar* bar = &(mock_desktop_bars[static_cast<int>(alignment)]);
  if (!bar->auto_hiding_enabled)
    return;
  if (thickness == bar->thickness)
    return;
  bar->thickness = thickness;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MockAutoHidingDesktopBarImpl::NotifyThicknessChange,
                 method_factory_.GetWeakPtr()));
}

void MockAutoHidingDesktopBarImpl::NotifyVisibilityChange(
    AutoHidingDesktopBar::Alignment alignment,
    AutoHidingDesktopBar::Visibility visibility) {
  observer_->OnAutoHidingDesktopBarVisibilityChanged(alignment, visibility);
}

void MockAutoHidingDesktopBarImpl::NotifyThicknessChange() {
  observer_->OnAutoHidingDesktopBarThicknessChanged();
}

bool ExistsPanel(Panel* panel) {
  const PanelManager::Panels& panels = PanelManager::GetInstance()->panels();
  return find(panels.begin(), panels.end(), panel) != panels.end();
}

}  // namespace

const FilePath::CharType* BasePanelBrowserTest::kTestDir =
    FILE_PATH_LITERAL("panels");

BasePanelBrowserTest::BasePanelBrowserTest()
    : InProcessBrowserTest(),
      testing_work_area_(0, 0, kTestingWorkAreaWidth,
                         kTestingWorkAreaHeight) {
#if defined(OS_MACOSX)
  FindBarBridge::disable_animations_during_testing_ = true;
#endif
}

BasePanelBrowserTest::~BasePanelBrowserTest() {
}

void BasePanelBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  EnableDOMAutomation();
}

void BasePanelBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  // Setup the work area and desktop bar so that we have consistent testing
  // environment for all panel related tests.
  PanelManager* panel_manager = PanelManager::GetInstance();
  mock_auto_hiding_desktop_bar_ = new MockAutoHidingDesktopBarImpl(
      panel_manager);
  panel_manager->set_auto_hiding_desktop_bar(mock_auto_hiding_desktop_bar_);
  // Do not use the testing work area if it is empty since we're going to
  // use the actual work area in some testing scenarios.
  if (!testing_work_area_.IsEmpty())
    panel_manager->SetWorkAreaForTesting(testing_work_area_);
  panel_manager->enable_auto_sizing(false);
  panel_manager->remove_delays_for_testing();
  // This is needed so the subsequently created panels can be activated.
  // On a Mac, it transforms background-only test process into foreground one.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
}

void BasePanelBrowserTest::WaitForPanelAdded(Panel* panel) {
  if (ExistsPanel(panel))
    return;
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_ADDED,
      content::Source<Panel>(panel));
  signal.Wait();
  EXPECT_TRUE(ExistsPanel(panel));
}

void BasePanelBrowserTest::WaitForPanelRemoved(Panel* panel) {
  if (!ExistsPanel(panel))
    return;
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_REMOVED,
      content::Source<Panel>(panel));
  signal.Wait();
  EXPECT_FALSE(ExistsPanel(panel));
}

void BasePanelBrowserTest::WaitForPanelActiveState(
    Panel* panel, ActiveState expected_state) {
  DCHECK(expected_state == SHOW_AS_ACTIVE ||
         expected_state == SHOW_AS_INACTIVE);
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(panel));
  if (panel->IsActive() == (expected_state == SHOW_AS_ACTIVE))
    return;  // Already in required state.
  signal.Wait();
  // Verify that transition happened in the desired direction.
  EXPECT_TRUE(panel->IsActive() == (expected_state == SHOW_AS_ACTIVE));
}

void BasePanelBrowserTest::WaitForWindowSizeAvailable(Panel* panel) {
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_WINDOW_SIZE_KNOWN,
      content::Source<Panel>(panel));
  if (panel_testing->IsWindowSizeKnown())
    return;
  signal.Wait();
  EXPECT_TRUE(panel_testing->IsWindowSizeKnown());
}

void BasePanelBrowserTest::WaitForBoundsAnimationFinished(Panel* panel) {
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel));
  if (!panel_testing->IsAnimatingBounds())
    return;
  signal.Wait();
  EXPECT_TRUE(!panel_testing->IsAnimatingBounds());
}

Panel* BasePanelBrowserTest::CreatePanelWithParams(
    const CreatePanelParams& params) {
#if defined(OS_MACOSX)
  // Opening panels on a Mac causes NSWindowController of the Panel window
  // to be autoreleased. We need a pool drained after it's done so the test
  // can close correctly. The NSWindowController of the Panel window controls
  // lifetime of the Browser object so we want to release it as soon as
  // possible. In real Chrome, this is done by message pump.
  // On non-Mac platform, this is an empty class.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 params.name,
                                                 params.bounds,
                                                 browser()->profile());
  EXPECT_TRUE(panel_browser->is_type_panel());

  if (!params.url.is_empty()) {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    panel_browser->AddSelectedTabWithURL(params.url,
                                         content::PAGE_TRANSITION_START_PAGE);
    observer.Wait();
  }

  Panel* panel = static_cast<Panel*>(panel_browser->window());

  if (params.bounds.width() || params.bounds.height())
    EXPECT_FALSE(panel->auto_resizable());
  else
    EXPECT_TRUE(panel->auto_resizable());

  if (params.show_flag == SHOW_AS_ACTIVE) {
    panel->Show();
  } else {
#if defined(OS_LINUX)
    // On bots, we might have a simple window manager which always activates new
    // windows, and can't always deactivate them. Activate previously active
    // window back to ensure the new window is inactive.
    if (ui::GuessWindowManager() == ui::WM_ICE_WM) {
      Browser* last_active_browser = BrowserList::GetLastActive();
      EXPECT_TRUE(last_active_browser);
      EXPECT_NE(last_active_browser, panel->browser());
      panel->ShowInactive();  // Shows as active anyways in icewm.
      MessageLoopForUI::current()->RunAllPending();
      // Restore focus where it was. It will deactivate the new panel.
      last_active_browser->window()->Activate();
    } else {
      panel->ShowInactive();
    }
#else
    panel->ShowInactive();
#endif
  }
  MessageLoopForUI::current()->RunAllPending();
  // More waiting, because gaining or losing focus may require inter-process
  // asynchronous communication, and it is not enough to just run the local
  // message loop to make sure this activity has completed.
  WaitForPanelActiveState(panel, params.show_flag);

  // On Linux, window size is not available right away and we should wait
  // before moving forward with the test.
  WaitForWindowSizeAvailable(panel);

  // Wait for the bounds animations on creation to finish.
  WaitForBoundsAnimationFinished(panel);

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

void BasePanelBrowserTest::CreateTestTabContents(Browser* browser) {
  TabContentsWrapper* tab_contents =
      new TabContentsWrapper(new TestTabContents(browser->profile(), NULL));
  browser->AddTab(tab_contents, content::PAGE_TRANSITION_LINK);
}

scoped_refptr<Extension> BasePanelBrowserTest::CreateExtension(
    const FilePath::StringType& path,
    Extension::Location location,
    const DictionaryValue& extra_value) {
#if defined(OS_WIN)
  FilePath full_path(FILE_PATH_LITERAL("c:\\"));
#else
  FilePath full_path(FILE_PATH_LITERAL("/"));
#endif
  full_path = full_path.Append(path);

  scoped_ptr<DictionaryValue> input_value(extra_value.DeepCopy());
  input_value->SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  input_value->SetString(extension_manifest_keys::kName, "Sample Extension");

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      full_path,  location, *input_value,
      Extension::STRICT_ERROR_CHECKS, &error);
  EXPECT_TRUE(extension.get());
  EXPECT_STREQ("", error.c_str());
  browser()->GetProfile()->GetExtensionService()->
      OnExtensionInstalled(extension.get(), false, -1);
  return extension;
}
