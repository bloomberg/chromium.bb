// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/overflow_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/panels/panel_overflow_indicator.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#include "chrome/browser/ui/panels/test_panel_mouse_watcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// We override the default value for testing purpose.
const int kMaxVisibleOverflowPanelsForTesting = 3;
const int kMaxVisibleOverflowPanelsOnHoverForTesting = 6;

// Encapsulates all the info we need to verify if a panel behaves as expected
// when we do the overflow testing.
struct PanelData {
  Panel* panel;
  PanelStrip::Type layout_type;
  Panel::ExpansionState expansion_state;
  bool visible;
  bool active;

  explicit PanelData(Panel* panel)
      : panel(panel),
        layout_type(panel->panel_strip()->type()),
        expansion_state(panel->expansion_state()),
        visible(!panel->GetBounds().IsEmpty()),
        active(panel->IsActive()) {
  }

  PanelData(Panel* panel, PanelStrip::Type layout_type,
            Panel::ExpansionState expansion_state,
            bool visible, bool active)
      : panel(panel),
        layout_type(layout_type),
        expansion_state(expansion_state),
        visible(visible),
        active(active) {
  }

  bool operator==(const PanelData& another) const {
    return panel == another.panel &&
           layout_type == another.layout_type &&
           expansion_state == another.expansion_state &&
           visible == another.visible &&
           active == another.active;
  }

  bool operator!=(const PanelData& another) const {
    return !(*this == another);
  }
};

// For gtest printing.
::std::ostream& operator<<(::std::ostream& os, const PanelData& data);
::std::ostream& operator<<(::std::ostream& os, const PanelData& data) {
  return os << "("
            << data.panel->browser()->app_name() << ", "
            << data.layout_type << ", "
            << data.expansion_state << ", "
            << data.visible << ", "
            << data.active << ")";
}


class PanelDataList : public std::vector<PanelData> {
 public:
  void AddDocked(Panel* panel, Panel::ExpansionState expansion_state,
                 bool active) {
    push_back(PanelData(panel, PanelStrip::DOCKED, expansion_state,
                        true,  // docked is always visible
                        active));
  }

  void AddOverflow(Panel* panel, bool visible) {
    push_back(PanelData(panel, PanelStrip::IN_OVERFLOW,
                        panel->expansion_state(), visible,
                        false));  // overflow is never active
  }
};

}  // namespace

class PanelOverflowBrowserTest : public BasePanelBrowserTest {
 public:
  PanelOverflowBrowserTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelOverflowBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    PanelManager* panel_manager = PanelManager::GetInstance();
    panel_manager->overflow_strip()->set_max_visible_panels(
        kMaxVisibleOverflowPanelsForTesting);
    panel_manager->overflow_strip()->set_max_visible_panels_on_hover(
        kMaxVisibleOverflowPanelsOnHoverForTesting);

    PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
    panel_manager->SetMouseWatcherForTesting(mouse_watcher);

    // All the overflow tests assume 800x600 work area. Do the check now.
    gfx::Rect display_area = PanelManager::GetInstance()->
        display_settings_provider()->GetDisplayArea();
    DCHECK(display_area.width() == 800);
    DCHECK(display_area.height() == 600);
  }

 protected:
  static PanelDataList GetAllDockedPanelData() {
    PanelDataList panel_data_list;
    DockedPanelStrip::Panels panels =
        PanelManager::GetInstance()->docked_strip()->panels();
    for (DockedPanelStrip::Panels::const_iterator iter = panels.begin();
         iter != panels.end(); ++iter) {
      Panel* panel = *iter;
      panel_data_list.push_back(PanelData(panel));
    }
    return panel_data_list;
  }

  static PanelDataList GetAllOverflowPanelData() {
    PanelDataList panel_data_list;
    OverflowPanelStrip::Panels panels =
        PanelManager::GetInstance()->overflow_strip()->panels();
    for (OverflowPanelStrip::Panels::const_iterator iter = panels.begin();
         iter != panels.end(); ++iter) {
      Panel* panel = *iter;
      panel_data_list.push_back(PanelData(panel));
    }
    return panel_data_list;
  }

  static void MoveMouseAndWaitForOverflowAnimationEnded(
      const gfx::Point& position) {
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
        content::Source<OverflowPanelStrip>(
            PanelManager::GetInstance()->overflow_strip()));
    MoveMouse(position);
    signal.Wait();
  }

  static bool IsPanelVisible(Panel* panel) {
    return !panel->GetBounds().IsEmpty();
  }

  static bool IsPanelInOverflowStrip(Panel* panel) {
    OverflowPanelStrip* overflow_strip =
        PanelManager::GetInstance()->overflow_strip();
    int expected_x = overflow_strip->display_area().x();
    gfx::Size expected_size(overflow_strip->current_display_width(),
                            panel->IconOnlySize().height());
    gfx::Rect bounds = panel->GetBounds();
    EXPECT_EQ(expected_x, bounds.x());
    EXPECT_EQ(expected_size, bounds.size());
    return expected_x == bounds.x() && expected_size == bounds.size();
  }

  std::vector<Panel*> CreateOverflowPanels(int num_docked_panels,
                                           int num_overflow_panels,
                                           const int* panel_widths) {
    const int kTestPanelHeight = 200;
    std::vector<Panel*> panels;

    // First, create panels to fill the docked strip.
    int i = 0;
    for (; i < num_docked_panels; ++i) {
      CreatePanelParams params(
          MakePanelName(i),
          gfx::Rect(0, 0, panel_widths[i], kTestPanelHeight),
          SHOW_AS_INACTIVE);
      Panel* panel = CreatePanelWithParams(params);
      panels.push_back(panel);
    }

    // Then, create panels that would be placed in the overflow strip.
    int num_panels = num_docked_panels + num_overflow_panels;
    for (; i < num_panels; ++i) {
      CreatePanelParams params(
          MakePanelName(i),
          gfx::Rect(0, 0, panel_widths[i], kTestPanelHeight),
          SHOW_AS_INACTIVE);
      Panel* panel = CreatePanelWithParams(params);
      WaitForLayoutModeChanged(panel, PanelStrip::IN_OVERFLOW);
      panels.push_back(panel);
    }

    return panels;
  }
};

// TODO(dimich): remove the guard when overflow indicator is implemented on
// other platforms.
#if defined(OS_MACOSX)
#define MAYBE_CreateMoreOverflowPanels DISABLED_CreateMoreOverflowPanels
#define MAYBE_OverflowIndicatorCount DISABLED_OverflowIndicatorCount
#define MAYBE_DrawOverflowAttention DISABLED_DrawOverflowAttention
#else
#define MAYBE_CreateMoreOverflowPanels CreateMoreOverflowPanels
#define MAYBE_OverflowIndicatorCount OverflowIndicatorCount
#define MAYBE_DrawOverflowAttention DrawOverflowAttention
#endif

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, CheckPanelProperties) {
  // Create 3 panels that fit.
  Panel* panel1 = CreatePanelWithBounds("1", gfx::Rect(0, 0, 250, 200));
  Panel* panel2 = CreatePanelWithBounds("2", gfx::Rect(0, 0, 300, 200));
  Panel* panel3 = CreatePanelWithBounds("3", gfx::Rect(0, 0, 200, 200));

  // Create an overflow panel without waiting for it to be moved to overflow.
  // Check its properties before and after it is moved to overflow.
  CreatePanelParams params(
      "4", gfx::Rect(0, 0, 255, 200), SHOW_AS_INACTIVE);
  params.wait_for_fully_created = false;
  Panel* panel4 = CreatePanelWithParams(params);

  EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel3->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel4->expansion_state());

  EXPECT_FALSE(panel1->has_temporary_layout());
  EXPECT_FALSE(panel2->has_temporary_layout());
  EXPECT_FALSE(panel3->has_temporary_layout());
  EXPECT_TRUE(panel4->has_temporary_layout());

  EXPECT_TRUE(panel1->draggable());
  EXPECT_TRUE(panel2->draggable());
  EXPECT_TRUE(panel3->draggable());
  EXPECT_FALSE(panel4->draggable());

  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel1->attention_mode());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel2->attention_mode());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel3->attention_mode());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel4->attention_mode());

  EXPECT_FALSE(panel1->CanResizeByMouse());
  EXPECT_FALSE(panel2->CanResizeByMouse());
  EXPECT_FALSE(panel3->CanResizeByMouse());
  EXPECT_FALSE(panel4->CanResizeByMouse());

  // Make sure last panel really did overflow.
  WaitForLayoutModeChanged(panel4, PanelStrip::IN_OVERFLOW);
  EXPECT_FALSE(panel4->has_temporary_layout());
  EXPECT_FALSE(panel4->draggable());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel4->attention_mode());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, UpdateDraggableStatus) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreatePanel("panel");
  EXPECT_TRUE(panel->draggable());
  panel_manager->MovePanelToStrip(panel,
                                  PanelStrip::IN_OVERFLOW,
                                  PanelStrip::DEFAULT_POSITION);
  EXPECT_FALSE(panel->draggable());
  panel_manager->MovePanelToStrip(panel,
                                  PanelStrip::DOCKED,
                                  PanelStrip::DEFAULT_POSITION);
  EXPECT_TRUE(panel->draggable());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, CreateOverflowPanels) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();

  EXPECT_EQ(0, panel_manager->num_panels());
  EXPECT_EQ(0, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());
  EXPECT_FALSE(overflow_strip->overflow_indicator());

  // Create 3 docked panels.
  Panel* panel0 = CreatePanelWithBounds("Panel0", gfx::Rect(0, 0, 250, 200));
  Panel* panel1 = CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 260, 200));
  Panel* panel2 = CreatePanelWithBounds("Panel2", gfx::Rect(0, 0, 200, 200));

  EXPECT_EQ(3, panel_manager->num_panels());
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());
  EXPECT_FALSE(overflow_strip->overflow_indicator());
  EXPECT_EQ(Panel::EXPANDED, panel0->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());

  // Create 1 overflow panel.
  CreatePanelParams params(
      "Panel3", gfx::Rect(0, 0, 255, 200), SHOW_AS_INACTIVE);
  Panel* panel3 = CreatePanelWithParams(params);
  WaitForLayoutModeChanged(panel3, PanelStrip::IN_OVERFLOW);

  EXPECT_EQ(4, panel_manager->num_panels());
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_FALSE(overflow_strip->overflow_indicator());
  EXPECT_TRUE(IsPanelVisible(panel3));
  EXPECT_TRUE(IsPanelInOverflowStrip(panel3));
  EXPECT_EQ(gfx::Size(255, 200), panel3->restored_size());

  // Create 1 more overflow panel, make it active initially.
  params.name = "Panel4";
  params.show_flag = SHOW_AS_ACTIVE;
  params.expected_active_state = SHOW_AS_INACTIVE;
  Panel* panel4 = CreatePanelWithParams(params);
  WaitForLayoutModeChanged(panel4, PanelStrip::IN_OVERFLOW);

  EXPECT_EQ(5, panel_manager->num_panels());
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(2, overflow_strip->num_panels());
  EXPECT_FALSE(overflow_strip->overflow_indicator());
  EXPECT_TRUE(IsPanelVisible(panel4));
  EXPECT_TRUE(IsPanelInOverflowStrip(panel4));
  EXPECT_FALSE(panel4->IsActive());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest,
                       MAYBE_CreateMoreOverflowPanels) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();

  const int panel_widths[] = {
      250, 260, 200,  // docked
      255, 220, 210,  // overflow
      220, 230        // overflow-on-overflow
  };
  CreateOverflowPanels(3, 5, panel_widths);

  std::vector<Panel*> panels = panel_manager->panels();
  ASSERT_EQ(8u, panels.size());
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(5, overflow_strip->num_panels());
  EXPECT_EQ(2, overflow_strip->overflow_indicator()->GetCount());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panels[3]->panel_strip()->type());
  EXPECT_TRUE(IsPanelVisible(panels[3]));
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panels[4]->panel_strip()->type());
  EXPECT_TRUE(IsPanelVisible(panels[4]));
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panels[5]->panel_strip()->type());
  EXPECT_TRUE(IsPanelVisible(panels[5]));
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panels[6]->panel_strip()->type());
  EXPECT_FALSE(IsPanelVisible(panels[6]));
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panels[7]->panel_strip()->type());
  EXPECT_FALSE(IsPanelVisible(panels[7]));

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, AddMinimizedTillOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  EXPECT_EQ(0, panel_manager->num_panels());
  EXPECT_EQ(0, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());

  CreatePanelParams params("Test", gfx::Rect(0, 0, 100, 100), SHOW_AS_INACTIVE);

  // We want our panels to be uninitialized at the beginning so that
  // we test minimizing panels in temp layout and its effect on
  // the minimized panel count.
  params.wait_for_fully_created = false;

  unsigned int num_panels_to_add = 10;
  unsigned int num_panels = 0;
  for (; num_panels < num_panels_to_add; ++num_panels) {
    Panel* panel = CreatePanelWithParams(params);
    panel->SetExpansionState(Panel::MINIMIZED);

    panel->panel_strip()->RemovePanel(panel);
    EXPECT_EQ(NULL, panel->panel_strip());
    panel->set_has_temporary_layout(false);

    docked_strip->AddPanel(panel, PanelStrip::DEFAULT_POSITION);
    EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
    EXPECT_FALSE(panel->has_temporary_layout());
  }

  EXPECT_EQ(num_panels_to_add, num_panels);
  EXPECT_EQ(0, docked_strip->num_temporary_layout_panels());
  EXPECT_EQ(num_panels, static_cast<unsigned int>
                (docked_strip->minimized_panel_count() +
                 overflow_strip->num_panels()));

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest,
                       CreatePanelOnDelayedOverflow) {
  // Create 2 big panels.
  CreatePanelWithBounds("Panel0", gfx::Rect(0, 0, 260, 200));
  CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 260, 200));

  // Create an overflow panel without waiting for it to be moved to overflow.
  CreatePanelParams params(
      "Panel2", gfx::Rect(0, 0, 255, 200), SHOW_AS_INACTIVE);
  params.wait_for_fully_created = false;
  Panel* overflow_panel = CreatePanelWithParams(params);
  EXPECT_EQ(Panel::EXPANDED, overflow_panel->expansion_state());
  EXPECT_TRUE(overflow_panel->has_temporary_layout());

  // Create a small panel that could fit within the available space in the
  // panel strip.
  CreatePanelParams params2(
      "Panel3", gfx::Rect(0, 0, 110, 200), SHOW_AS_INACTIVE);
  Panel* panel3 = CreatePanelWithParams(params2);
  EXPECT_EQ(Panel::EXPANDED, panel3->expansion_state());
  EXPECT_FALSE(panel3->has_temporary_layout());

  WaitForLayoutModeChanged(overflow_panel, PanelStrip::IN_OVERFLOW);
  EXPECT_FALSE(overflow_panel->has_temporary_layout());
  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, CloseOverflowPanels) {
  // Panels are unhidden when moving out of overflow-on-overflow.
  // IceWM always activates the window when shown.
  if (SkipTestIfIceWM())
    return;

  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5
  //   overflow-on-overflow: P6, P7
  int num_docked_panels = 3;
  int num_overflow_panels = 5;
  const int panel_widths[] = {
      260, 250, 200,  // docked
      255, 260, 120,  // overflow
      240, 210        // overflow-on-overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(
      num_docked_panels, num_overflow_panels, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[6], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close an overflow-on-overflow panel. Expect only that panel is closed.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5,
  //   overflow-on-overflow: P7
  CloseWindowAndWait(panels[6]->browser());
  num_overflow_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close an overflow panel. Expect an overflow-on-overflow panel to become
  // visible in the overflow strip.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P5, P7
  CloseWindowAndWait(panels[4]->browser());
  num_overflow_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[7], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close an overflow panel. Expect only that panel is closed.
  //   docked:               P0, P1, P2
  //   overflow:             P5, P7
  CloseWindowAndWait(panels[3]->browser());
  num_overflow_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[7], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, CloseDockedPanels) {
  // Panels are unhidden when moving out of overflow-on-overflow.
  // IceWM always activates the window when shown.
  if (SkipTestIfIceWM())
    return;

  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5
  //   overflow-on-overflow: P6, P7, P8
  int num_docked_panels = 3;
  int num_overflow_panels = 6;
  const int panel_widths[] = {
      260, 250, 200,  // docked
      255, 260, 120,  // overflow
      240, 210, 258   // overflow-on-overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(
      num_docked_panels, num_overflow_panels, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[6], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  expected_overflow_list.AddOverflow(panels[8], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close a docked panel. Expect an overflow panel to move over and an
  // overflow-on-overflow panel to become visible.
  //   docked:               P0, P2, P3
  //   overflow:             P4, P5, P6
  //   overflow-on-overflow: P7, P8
  CloseWindowAndWait(panels[1]->browser());
  num_overflow_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[6], true);
  expected_overflow_list.AddOverflow(panels[7], false);
  expected_overflow_list.AddOverflow(panels[8], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close another docked panel. Remaining overflow panels cannot move over
  // due to not enough room.
  //   docked:               P0, P3
  //   overflow:             P4, P5, P6
  //   overflow-on-overflow: P7, P8
  CloseWindowAndWait(panels[2]->browser());
  num_docked_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close one more docked panel. Expect two overflow panels to move over and
  // two overflow-on-overflow panels to become visible.
  //   docked:               P0, P4, P5
  //   overflow:             P6, P7, P8
  CloseWindowAndWait(panels[3]->browser());
  num_docked_panels++;
  num_overflow_panels -= 2;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[4], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[6], true);
  expected_overflow_list.AddOverflow(panels[7], true);
  expected_overflow_list.AddOverflow(panels[8], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Close another docked panel. Expect one overflow panel to move over.
  //   docked:               P4, P5, P6
  //   overflow:             P7, P8
  CloseWindowAndWait(panels[0]->browser());
  num_overflow_panels--;
  ASSERT_EQ(num_docked_panels + num_overflow_panels,
            panel_manager->num_panels());
  EXPECT_EQ(num_docked_panels, docked_strip->num_panels());
  EXPECT_EQ(num_overflow_panels, overflow_strip->num_panels());

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[4], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[6], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[7], true);
  expected_overflow_list.AddOverflow(panels[8], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, CloseWithDelayedOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();

  // Create 2 big panels.
  Panel* panel0 = CreatePanelWithBounds("Panel0", gfx::Rect(0, 0, 260, 200));
  Panel* panel1 = CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 260, 200));

  // Create an overflow panel without waiting for it to be moved to overflow.
  CreatePanelParams params(
      "Panel2", gfx::Rect(0, 0, 255, 200), SHOW_AS_INACTIVE);
  params.wait_for_fully_created = false;
  Panel* overflow_panel = CreatePanelWithParams(params);
  EXPECT_EQ(2, docked_strip->num_panels());
  EXPECT_EQ(1, docked_strip->num_temporary_layout_panels());
  EXPECT_TRUE(overflow_panel->has_temporary_layout());

  // Close the overflow panel while it is still in temporary layout.
  CloseWindowAndWait(overflow_panel->browser());
  EXPECT_EQ(2, docked_strip->num_panels());
  EXPECT_EQ(0, docked_strip->num_temporary_layout_panels());

  // Create another overflow panel without waiting for it to move to overflow.
  params.name = "Panel3";
  overflow_panel = CreatePanelWithParams(params);
  EXPECT_EQ(2, docked_strip->num_panels());
  EXPECT_EQ(1, docked_strip->num_temporary_layout_panels());
  EXPECT_TRUE(overflow_panel->has_temporary_layout());

  // Close one of the non-overflow panels. Expect the delayed overflow
  // panel will not be affected by the close.
  // Hack. Pretend to close panel by removing it directly. Cannot use
  // CloseWindowAndWait() here because it will allow the delayed overflow
  // to complete.
  docked_strip->RemovePanel(panel1);
  EXPECT_EQ(1, docked_strip->num_panels());
  EXPECT_EQ(1, docked_strip->num_temporary_layout_panels());
  EXPECT_TRUE(overflow_panel->has_temporary_layout());

  // Make sure the overflow panel actually moves to overflow.
  WaitForLayoutModeChanged(overflow_panel, PanelStrip::IN_OVERFLOW);
  EXPECT_EQ(0, docked_strip->num_temporary_layout_panels());

  // Hack. Put the "falsely closed" panel back into the panel strip
  // so we can properly close it to wrap up this test.
  docked_strip->AddPanel(panel1, PanelStrip::DEFAULT_POSITION);

  panel0->Close();
  panel1->Close();
  overflow_panel->Close();
}

// http://crbug.com/115568
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ActivateOverflowPanels ActivateOverflowPanels
#else
#define MAYBE_ActivateOverflowPanels DISABLED_ActivateOverflowPanels
#endif
IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, MAYBE_ActivateOverflowPanels) {
  // Compiz refuses to activate initially inactive windows.
  if (SkipTestIfCompizWM())
    return;

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5
  //   overflow-on-overflow: P6, P7
  const int panel_widths[] = {
      250, 260, 200,  // docked
      210, 260, 230,  // overflow
      255, 210        // overflow-on-overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 5, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[6], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate an overflow panel. Expect one docked panel is swapped into the
  // overflow strip.
  //   docked:               P0, P1, P3
  //   overflow:             P2, P4, P5
  //   overflow-on-overflow: P6, P7
  panels[3]->Activate();
  WaitForPanelActiveState(panels[3], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[3], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[2], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);
  expected_overflow_list.AddOverflow(panels[6], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate an overflow panel. Expect two docked panels are swapped into the
  // overflow strip and one overflow panel to become hidden.
  //   docked:               P0, P4
  //   overflow:             P1, P3, P2
  //   overflow-on-overflow: P5, P6, P7
  panels[4]->Activate();
  WaitForPanelActiveState(panels[4], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[4], PanelStrip::DOCKED);
  WaitForPanelActiveState(panels[3], SHOW_AS_INACTIVE);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[4], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[1], true);
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[2], true);
  expected_overflow_list.AddOverflow(panels[5], false);
  expected_overflow_list.AddOverflow(panels[6], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate an overflow-on-overflow panel. Expect one docked panel is swapped
  // into the overflow strip.
  //   docked:               P0, P6
  //   overflow:             P4, P1, P3,
  //   overflow-on-overflow: P2, P5, P7
  panels[6]->Activate();
  WaitForPanelActiveState(panels[6], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[6], PanelStrip::DOCKED);
  WaitForPanelActiveState(panels[4], SHOW_AS_INACTIVE);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[6], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[1], true);
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[2], false);
  expected_overflow_list.AddOverflow(panels[5], false);
  expected_overflow_list.AddOverflow(panels[7], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate an overflow-on-overflow panel. No docked panel is swapped
  // since there has already been enough space in the panel strip.
  //   docked:               P0, P6, P7
  //   overflow:             P4, P1, P3,
  //   overflow-on-overflow: P2, P5
  panels[7]->Activate();
  WaitForPanelActiveState(panels[7], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[7], PanelStrip::DOCKED);
  WaitForPanelActiveState(panels[6], SHOW_AS_INACTIVE);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[6], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[7], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[1], true);
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[2], false);
  expected_overflow_list.AddOverflow(panels[5], false);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest,
                       ActivateOverflowPanelWithClick) {
  // Compiz refuses to activate initially inactive windows.
  if (SkipTestIfCompizWM())
    return;

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  const int panel_widths[] = {
      250, 260, 200,  // docked
      200, 200,       // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 2, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate an overflow panel by clicking on the titlebar. Expect one
  // docked panel is swapped into the overflow strip.
  //   docked:               P0, P1, P3
  //   overflow:             P2, P4
  scoped_ptr<NativePanelTesting> test_panel3(
      NativePanelTesting::Create(panels[3]->native_panel()));
  test_panel3->PressLeftMouseButtonTitlebar(panels[3]->GetBounds().origin());
  test_panel3->ReleaseMouseButtonTitlebar();
  WaitForPanelActiveState(panels[3], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[3], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[2], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Activate another overflow panel, this time clicking on the titlebar
  // with the modifier to apply all. Expect the same behavior as without
  // the modifier.
  //   docked:               P0, P1, P4
  //   overflow:             P3, P2
  scoped_ptr<NativePanelTesting> test_panel4(
      NativePanelTesting::Create(panels[4]->native_panel()));
  test_panel4->PressLeftMouseButtonTitlebar(panels[4]->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel4->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  WaitForPanelActiveState(panels[4], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[4], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[4], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[2], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(
    PanelOverflowBrowserTest,
    MoveMinimizedPanelToOverflowAndBringBackByActivate) {
  // Compiz refuses to activate initially inactive windows.
  if (SkipTestIfCompizWM())
    return;

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  const int panel_widths[] = {
      250, 260, 200,  // docked
      210, 260        // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 2, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);

  // Minimize a docked panel and then bump it to overflow by activating an
  // overflow panel.
  //   docked:               P0, P1, P3
  //   overflow:             P2, P4
  panels[2]->SetExpansionState(Panel::MINIMIZED);
  panels[3]->Activate();
  WaitForPanelActiveState(panels[3], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[3], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[2], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Reactivate the formerly minimized panel. It will return to the panel
  // strip in expanded state.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  panels[2]->Activate();
  WaitForPanelActiveState(panels[2], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[2], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Minimize a panel to title only mode, then bump it to overflow.
  //   docked:               P0, P1, P3
  //   overflow:             P2, P4
  panels[2]->SetExpansionState(Panel::TITLE_ONLY);
  panels[3]->Activate();
  WaitForPanelActiveState(panels[3], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[3], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[2], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  // Reactivate the formerly minimized panel. It will return to the panel
  // strip in expanded state.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  panels[2]->Activate();
  WaitForPanelActiveState(panels[2], SHOW_AS_ACTIVE);
  WaitForLayoutModeChanged(panels[2], PanelStrip::DOCKED);

  expected_docked_list.clear();
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, true);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  expected_overflow_list.clear();
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(
    PanelOverflowBrowserTest,
    MoveMinimizedPanelToOverflowAndBringBackByCloseOrResize) {
  // IceWM activates windows whenever they're shown, causing panels
  // to activate as they move out of overflow-on-overflow.
  // Compiz refuses to activate an initially inactive window.
  if (SkipTestIfIceWM() || SkipTestIfCompizWM())
    return;

  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create docked and overflow panels.
  //   docked:   P0, P1, P2
  //   overflow: P3, P4, P5
  const int panel_widths[] = {
      240, 240, 120,  // docked
      240, 240, 240   // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 3, panel_widths);

  PanelDataList expected_docked_list;
  expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
  expected_docked_list.AddDocked(panels[2], Panel::EXPANDED, false);
  EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

  PanelDataList expected_overflow_list;
  expected_overflow_list.AddOverflow(panels[3], true);
  expected_overflow_list.AddOverflow(panels[4], true);
  expected_overflow_list.AddOverflow(panels[5], true);

  // Test case 1: restoring minimized to minimized.
  {
    // Minimize a docked panel and then bump it to overflow by activating an
    // overflow panel.
    //   docked:   P0, P1, P3
    //   overflow: P2, P4, P5
    panels[2]->SetExpansionState(Panel::MINIMIZED);
    panels[3]->Activate();
    WaitForPanelActiveState(panels[3], SHOW_AS_ACTIVE);
    WaitForLayoutModeChanged(panels[3], PanelStrip::DOCKED);

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[3], Panel::EXPANDED, true);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[2], true);
    expected_overflow_list.AddOverflow(panels[4], true);
    expected_overflow_list.AddOverflow(panels[5], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

    // Bring back the formerly minimized panel by closing a panel. It will
    // return to the panel strip icocoa/n the minimized state.
    //   docked:   P0, P1, P2
    //   overflow: P4, P5
    CloseWindowAndWait(panels[3]->browser());
    WaitForLayoutModeChanged(panels[2], PanelStrip::DOCKED);

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[2], Panel::MINIMIZED, false);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[4], true);
    expected_overflow_list.AddOverflow(panels[5], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());
  }

  // Test case 2: restoring minimized to title-only.
  {
    // Bump the minimized panel to overflow by activating an overflow panel.
    //   docked:   P0, P1, P4
    //   overflow: P2, P5
    panels[4]->Activate();
    WaitForPanelActiveState(panels[4], SHOW_AS_ACTIVE);
    WaitForLayoutModeChanged(panels[4], PanelStrip::DOCKED);

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[4], Panel::EXPANDED, true);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[2], true);
    expected_overflow_list.AddOverflow(panels[5], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

    // Minimize another panel and hover the mouse over it. This should bring up
    // all currently minimized panels. When a formerly minimized or title-only
    // panel is restored from the overflow area, it should also be title-only.
    panels[0]->SetExpansionState(Panel::MINIMIZED);
    MoveMouse(gfx::Point(panels[0]->GetBounds().x(),
                         panels[0]->GetBounds().y()));
    WaitForExpansionStateChanged(panels[0], Panel::TITLE_ONLY);

    // Bring back the formerly minimized panel by closing a panel. It will
    // return to the panel strip in the title-only state.
    //   docked:   P0, P1, P2
    //   overflow: P5
    CloseWindowAndWait(panels[4]->browser());
    WaitForLayoutModeChanged(panels[2], PanelStrip::DOCKED);

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::TITLE_ONLY, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[2], Panel::TITLE_ONLY, false);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[5], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());
  }

  // Test case 3: restoring title-only to title-only.
  {
    // Bump the title-only panel to overflow by activating an overflow panel.
    //   docked:   P0, P1, P5
    //   overflow: P2
    panels[5]->Activate();
    WaitForPanelActiveState(panels[5], SHOW_AS_ACTIVE);
    WaitForLayoutModeChanged(panels[5], PanelStrip::DOCKED);

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::TITLE_ONLY, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, true);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[2], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

    // Bring back the formerly title-only panel by shrinking a panel. It will
    // return to the panel strip in the title-only state.
    //   docked:   P0, P1, P5, P2
    panel_manager->ResizePanel(panels[5], gfx::Size(
        panels[5]->GetBounds().width() / 2,
        panels[5]->GetBounds().height() / 2));

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::TITLE_ONLY, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, true);
    expected_docked_list.AddDocked(panels[2], Panel::TITLE_ONLY, false);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());
  }

  // Test case 4: restoring title-only to minimized.
  {
    // Bump the minimized panel to overflow by enlarging a panel.
    //   docked:   P0, P1, P5
    //   overflow: P2
    panel_manager->ResizePanel(panels[5], gfx::Size(
        panels[5]->GetBounds().width() * 2,
        panels[5]->GetBounds().height() * 2));

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::TITLE_ONLY, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, true);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    expected_overflow_list.AddOverflow(panels[2], true);
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());

    // Move the mouse away. This should bring down all currently title-only
    // panels. When a formerly minimized or title-only panel is restored from
    // the overflow area, it should be minimized.
    MoveMouse(gfx::Point(0, 0));
    WaitForExpansionStateChanged(panels[0], Panel::MINIMIZED);

    // Bring back the formerly title-only panel by shrinking a panel. It will
    // return to the panel strip in the minimized state.
    //   docked:   P0, P1, P5, P2
    panel_manager->ResizePanel(panels[5], gfx::Size(
        panels[5]->GetBounds().width() / 2,
        panels[5]->GetBounds().height() / 2));

    expected_docked_list.clear();
    expected_docked_list.AddDocked(panels[0], Panel::MINIMIZED, false);
    expected_docked_list.AddDocked(panels[1], Panel::EXPANDED, false);
    expected_docked_list.AddDocked(panels[5], Panel::EXPANDED, true);
    expected_docked_list.AddDocked(panels[2], Panel::MINIMIZED, false);
    EXPECT_EQ(expected_docked_list, GetAllDockedPanelData());

    expected_overflow_list.clear();
    EXPECT_EQ(expected_overflow_list, GetAllOverflowPanelData());
  }

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest,
                       HoverOverOverflowAreaWithoutOverflowOfOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();
  int iconified_width = overflow_strip->current_display_width();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  const int panel_widths[] = {
      250, 260, 200,  // docked
      255, 220        // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 2, panel_widths);

  // Move mouse beyond the right edge of the top overflow panel.
  // Expect the overflow area remains shrunk.
  MoveMouse(gfx::Point(panels[4]->GetBounds().right() + 1,
                       panels[4]->GetBounds().y()));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse above the top overflow panel. Expect the overflow area
  // remains shrunk.
  MoveMouse(gfx::Point(panels[4]->GetBounds().x(),
                       panels[4]->GetBounds().y() - 1));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse below the bottom overflow panel. Expect the overflow area
  // remains shrunk.
  MoveMouse(gfx::Point(panels[3]->GetBounds().right(),
                       panels[3]->GetBounds().bottom() + 1));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse to the origin of an overflow panel. Expect the overflow area
  // gets expanded.
  MoveMouseAndWaitForOverflowAnimationEnded(
      panels[4]->GetBounds().origin());
  int hover_width = overflow_strip->current_display_width();
  EXPECT_GT(hover_width, iconified_width);
  EXPECT_EQ(hover_width, panels[3]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[4]->GetBounds().width());

  // Move mouse to the origin of another overflow panel. Expect the overflow
  // area remains expanded.
  MoveMouse(panels[3]->GetBounds().origin());
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse beyond the left edge of an overflow panel. Expect the overflow
  // area remains expanded.
  MoveMouse(gfx::Point(panels[4]->GetBounds().x() - 5,
                       panels[4]->GetBounds().y()));
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse to the bottom-right corner of the bottom overflow panel.
  // Expect the overflow area remains expanded.
  MoveMouse(gfx::Point(panels[3]->GetBounds().right(),
                       panels[3]->GetBounds().bottom()));
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse beyond the right edge of the hover panel. Expect the overflow
  // area gets shrunk.
  MoveMouseAndWaitForOverflowAnimationEnded(gfx::Point(
      panels[4]->GetBounds().right() + 5, panels[4]->GetBounds().y()));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());
  EXPECT_EQ(iconified_width, panels[3]->GetBounds().width());
  EXPECT_EQ(iconified_width, panels[4]->GetBounds().width());

  panel_manager->CloseAll();
}

// http://crbug.com/115568
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_HoverOverOverflowAreaWithOverflowOfOverflow \
    HoverOverOverflowAreaWithOverflowOfOverflow
#else
#define MAYBE_HoverOverOverflowAreaWithOverflowOfOverflow \
    DISABLED_HoverOverOverflowAreaWithOverflowOfOverflow
#endif
IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest,
                       MAYBE_HoverOverOverflowAreaWithOverflowOfOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  OverflowPanelStrip* overflow_strip =
      panel_manager->overflow_strip();
  int iconified_width = overflow_strip->current_display_width();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5
  //   overflow-on-overflow: P6, P7
  const int panel_widths[] = {
      250, 260, 200,  // docked
      255, 220, 260,  // overflow
      140, 210        // overflow-on-overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 5, panel_widths);

  // Move mouse beyond the right edge of the top overflow panel.
  // Expect the overflow area remains shrunk.
  MoveMouse(gfx::Point(panels[5]->GetBounds().right() + 1,
                       panels[5]->GetBounds().y()));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse above the top overflow panel. Expect the overflow area
  // remains shrunk.
  MoveMouse(gfx::Point(panels[5]->GetBounds().x(),
                       panels[5]->GetBounds().y() - 1));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse below the bottom overflow panel. Expect the overflow area
  // remains shrunk.
  MoveMouse(gfx::Point(panels[3]->GetBounds().right(),
                       panels[3]->GetBounds().bottom() + 1));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // Move mouse to the origin of an overflow panel. Expect the overflow area
  // gets expanded.
  MoveMouseAndWaitForOverflowAnimationEnded(
      panels[4]->GetBounds().origin());
  int hover_width = overflow_strip->current_display_width();
  EXPECT_GT(hover_width, iconified_width);
  EXPECT_EQ(hover_width, panels[3]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[4]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[5]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[6]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[7]->GetBounds().width());

  // Move mouse to the origin of another overflow panel. Expect the overflow
  // area remains expanded.
  MoveMouse(panels[5]->GetBounds().origin());
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse beyond the left edge of an overflow panel. Expect the overflow
  // area remains expanded.
  MoveMouse(gfx::Point(panels[5]->GetBounds().x() - 5,
                       panels[5]->GetBounds().y()));
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse to the origin of the top overflow-on-overflow panel. Expect the
  // overflow area remains expanded.
  MoveMouse(panels[7]->GetBounds().origin());
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse to the bottom-right corner of the bottom overflow panel.
  // Expect the overflow area remains expanded.
  MoveMouse(gfx::Point(panels[3]->GetBounds().right(),
                       panels[3]->GetBounds().bottom()));
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());

  // Move mouse beyond the right edge of the hover panel. Expect the overflow
  // area gets shrunk.
  MoveMouseAndWaitForOverflowAnimationEnded(gfx::Point(
      panels[4]->GetBounds().right() + 5, panels[4]->GetBounds().y()));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());
  EXPECT_EQ(iconified_width, panels[3]->GetBounds().width());
  EXPECT_EQ(iconified_width, panels[4]->GetBounds().width());
  EXPECT_EQ(iconified_width, panels[5]->GetBounds().width());
  EXPECT_EQ(0, panels[6]->GetBounds().width());
  EXPECT_EQ(0, panels[7]->GetBounds().width());

  // Move mouse beyond the left edge of an overflow panel. Expect the overflow
  // area gets expanded.
  MoveMouseAndWaitForOverflowAnimationEnded(gfx::Point(
      panels[4]->GetBounds().x() - 5, panels[4]->GetBounds().y()));
  EXPECT_EQ(hover_width, overflow_strip->current_display_width());
  EXPECT_EQ(hover_width, panels[3]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[4]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[5]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[6]->GetBounds().width());
  EXPECT_EQ(hover_width, panels[7]->GetBounds().width());

  // Move mouse above the top overflow-on-overflow panel. Expect the overflow
  // area gets shrunk.
  MoveMouseAndWaitForOverflowAnimationEnded(gfx::Point(
      panels[7]->GetBounds().x(), panels[7]->GetBounds().y() - 1));
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());
  EXPECT_EQ(iconified_width, panels[3]->GetBounds().width());
  EXPECT_EQ(iconified_width, panels[4]->GetBounds().width());
  EXPECT_EQ(iconified_width, panels[5]->GetBounds().width());
  EXPECT_EQ(0, panels[6]->GetBounds().width());
  EXPECT_EQ(0, panels[7]->GetBounds().width());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, ResizePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  panel_manager->enable_auto_sizing(true);
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  // Create 4 panels that fit.
  // docked: P1 (250), P2 (200), P3 (100), P4 (100)
  // overflow: empty
  Panel* panel1 = CreatePanelWithBounds("1", gfx::Rect(0, 0, 250, 200));
  Panel* panel2 = CreatePanelWithBounds("2", gfx::Rect(0, 0, 200, 200));
  Panel* panel3 = CreatePanelWithBounds("3", gfx::Rect(0, 0, 100, 200));
  Panel* panel4 = CreatePanelWithBounds("4", gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());

  // Resize last panel so that it is too big to fit and overflows.
  // docked: P1 (250), P2 (200), P3 (100)
  // overflow: P4 (250)*
  gfx::Size new_size(250, 200);
  panel_manager->OnWindowAutoResized(panel4, new_size);
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());
  EXPECT_TRUE(IsPanelInOverflowStrip(panel4));
  EXPECT_EQ(new_size, panel4->restored_size());

  // Open another panel that will fit.
  // docked: P1 (250), P2 (200), P3 (100), P5 (100)*
  // overflow: P4 (250)
  Panel* panel5 = CreatePanelWithBounds("5", gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(Panel::EXPANDED, panel5->expansion_state());
  EXPECT_EQ(PanelStrip::DOCKED, panel5->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());  // same

  // Resize a panel from the middle of the strip so that it causes a
  // panel to overflow.
  // docked: P1 (250), P2 (200), P3 (250)*
  // overflow: P5 (100), P4 (250)
  panel_manager->OnWindowAutoResized(panel3, gfx::Size(250, 200));
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(2, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel5->panel_strip()->type());
  const OverflowPanelStrip::Panels& overflow = overflow_strip->panels();
  EXPECT_EQ(panel5, overflow[0]);  // new overflow panel is first
  EXPECT_EQ(panel4, overflow[1]);

  // Resize panel smaller so that panel from overflow can fit.
  // docked: P1 (250), P2 (200), P3 (100)*, P5 (100)
  // overflow: P4 (250)
  panel_manager->OnWindowAutoResized(panel3, gfx::Size(100, 200));
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel5->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());

  // Resize smaller again but not small enough to fit overflow panel.
  // docked: P1 (250), P2 (100)*, P3 (100), P5 (100)
  // overflow: P4 (250)
  panel_manager->OnWindowAutoResized(panel2, gfx::Size(100, 200));
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());  // same

  // Resize overflow panel bigger. It should stay in overflow and bounds
  // should not change.
  // docked: P1 (250), P2 (100), P3 (100), P5 (100)
  // overflow: P4 (251)*
  gfx::Rect bounds_before_resize = panel4->GetBounds();
  new_size.SetSize(251, 200);
  panel_manager->OnWindowAutoResized(panel4, new_size);
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());
  EXPECT_EQ(bounds_before_resize, panel4->GetBounds());
  EXPECT_EQ(new_size, panel4->restored_size());

  // Resize overflow panel to make it fit.
  // docked: P1 (250), P2 (100), P3 (100), P5 (100), P4 (100)*
  // overflow: empty
  panel_manager->OnWindowAutoResized(panel4, gfx::Size(100, 200));
  EXPECT_EQ(5, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel4->panel_strip()->type());

  // Resize a panel bigger, but not enough to cause overflow.
  // docked: P1 (250), P2 (100), P3 (150)*, P5 (100), P4 (100)
  // overflow: empty
  panel_manager->OnWindowAutoResized(panel3, gfx::Size(150, 200));
  EXPECT_EQ(5, docked_strip->num_panels());
  EXPECT_EQ(0, overflow_strip->num_panels());

  // Resize a panel to bump more than one panel to overflow.
  // docked: P1 (250), P2 (250)*, P3 (150)
  // overflow: P5 (100), P4 (100)
  panel_manager->OnWindowAutoResized(panel2, gfx::Size(250, 200));
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(2, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel5->panel_strip()->type());
  const OverflowPanelStrip::Panels& overflow2 = overflow_strip->panels();
  EXPECT_EQ(panel5, overflow2[0]);  // strip order is preserved
  EXPECT_EQ(panel4, overflow2[1]);

  panel1->Close();
  panel2->Close();
  panel3->Close();
  panel4->Close();
  panel5->Close();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, MAYBE_OverflowIndicatorCount) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5
  const int panel_widths[] = {
      250, 200, 250,  // docked
      250, 250, 260   // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 3, panel_widths);
  EXPECT_EQ(3, overflow_strip->num_panels());
  EXPECT_FALSE(overflow_strip->overflow_indicator());

  // Create 5 overflow-on-overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5, (P6, P7, P8, P9, P10)
  // The panels enclosed in parentheses are hidden.
  int num_existing_panels = panel_manager->num_panels();
  for (int i = 0; i < 5; ++i) {
    CreatePanelParams params(
        MakePanelName(num_existing_panels + i),
        gfx::Rect(0, 0, 250, 200),
        SHOW_AS_INACTIVE);
    Panel* panel = CreatePanelWithParams(params);
    WaitForLayoutModeChanged(panel, PanelStrip::IN_OVERFLOW);
    EXPECT_EQ(i + 1, overflow_strip->overflow_indicator()->GetCount());
    panels.push_back(panel);
  }

  // Expand the overflow area by moving mouse over it.
  // Expect the overflow indicator count gets updated.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4, P5, P6, P7, P8, (P9, P10)
  MoveMouseAndWaitForOverflowAnimationEnded(panels[3]->GetBounds().origin());
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_TRUE(IsPanelVisible(panels[7]));
  EXPECT_TRUE(IsPanelVisible(panels[8]));
  EXPECT_FALSE(IsPanelVisible(panels[9]));
  EXPECT_FALSE(IsPanelVisible(panels[10]));
  EXPECT_EQ(2, overflow_strip->overflow_indicator()->GetCount());

  // Close an overflow panel that makes one overflow-on-overflow panel become
  // visible. Expect the overflow indicator count gets decreased by 1.
  //   docked:               P0, P1, P2
  //   overflow:             P4, P5, P6, P7, P8, P9, (P10)
  CloseWindowAndWait(panels[3]->browser());
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_TRUE(IsPanelVisible(panels[7]));
  EXPECT_TRUE(IsPanelVisible(panels[8]));
  EXPECT_TRUE(IsPanelVisible(panels[9]));
  EXPECT_FALSE(IsPanelVisible(panels[10]));
  EXPECT_EQ(1, overflow_strip->overflow_indicator()->GetCount());

  // Shrink the overflow area by stopping hovering the mouse over the overflow
  // area. Expect the overflow indicator count gets updated.
  //   docked:               P0, P1, P2
  //   overflow:             P4, P5, P6, (P7, P8, P9, P10)
  MoveMouseAndWaitForOverflowAnimationEnded(gfx::Point(
      panels[4]->GetBounds().right() + 5, panels[4]->GetBounds().y()));
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_FALSE(IsPanelVisible(panels[7]));
  EXPECT_FALSE(IsPanelVisible(panels[8]));
  EXPECT_FALSE(IsPanelVisible(panels[9]));
  EXPECT_FALSE(IsPanelVisible(panels[10]));
  EXPECT_EQ(4, overflow_strip->overflow_indicator()->GetCount());

  // Close an overflow panel.
  // Expect the overflow indicator count gets decreased by 1.
  //   docked:               P0, P1, P2
  //   overflow:             P5, P6, P7, (P8, P9, P10)
  CloseWindowAndWait(panels[4]->browser());
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_TRUE(IsPanelVisible(panels[7]));
  EXPECT_FALSE(IsPanelVisible(panels[8]));
  EXPECT_FALSE(IsPanelVisible(panels[9]));
  EXPECT_FALSE(IsPanelVisible(panels[10]));
  EXPECT_EQ(3, overflow_strip->overflow_indicator()->GetCount());

  // Widen a docked panel to bump a panel to overflow and also get the
  // top visible overflow panel bumped to the overflow-to-overflow.
  // Expect the overflow indicator count gets increased by 1.
  //   docked:               P0, P1
  //   overflow:             P2, P5, P6, (P7, P8, P9, P10)
  gfx::Size larger_size = panels[1]->GetBounds().size();
  larger_size.Enlarge(50, 50);
  panel_manager->ResizePanel(panels[1], larger_size);
  WaitForBoundsAnimationFinished(panels[1]);
  WaitForLayoutModeChanged(panels[2], PanelStrip::IN_OVERFLOW);
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_FALSE(IsPanelVisible(panels[7]));
  EXPECT_FALSE(IsPanelVisible(panels[8]));
  EXPECT_FALSE(IsPanelVisible(panels[9]));
  EXPECT_FALSE(IsPanelVisible(panels[10]));
  EXPECT_EQ(4, overflow_strip->overflow_indicator()->GetCount());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, MAYBE_DrawOverflowAttention) {
  // IceWM may activate another panel when an overflow-on-overflow
  // panel is hidden. It happens to pick the panel we want to draw attention
  // to, but drawing attention on an active panel is a no-op, so this test
  // fails under IceWM.
  if (SkipTestIfIceWM())
    return;

  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, P8, P9, P10, P11)
  // The panels enclosed in parentheses are hidden.
  const int panel_widths[] = {
      100, 210, 210, 210,  // docked
      210, 210, 210,       // overflow
      210, 210, 210,       // overflow-on-overflow on shrunk
      210, 210             // overflow-on-overflow on expanded
  };
  std::vector<Panel*> panels = CreateOverflowPanels(4, 8, panel_widths);
  EXPECT_EQ(4, docked_strip->num_panels());
  EXPECT_EQ(8, overflow_strip->num_panels());
  PanelOverflowIndicator* overflow_indicator =
      overflow_strip->overflow_indicator();

  // Draw attention for a visible overflow panel.
  // Expect no impact to the overflow indicator.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, *P5, P6, (P7, P8, P9, P10, P11)
  EXPECT_FALSE(panels[5]->IsDrawingAttention());
  panels[5]->FlashFrame(true);
  EXPECT_TRUE(panels[5]->IsDrawingAttention());
  EXPECT_FALSE(overflow_indicator->IsDrawingAttention());

  // Stop drawing attention for the visible overflow panel.
  // Expect no impact to the overflow indicator.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, P8, P9, P10, P11)
  panels[5]->FlashFrame(false);
  EXPECT_FALSE(panels[5]->IsDrawingAttention());
  EXPECT_FALSE(overflow_indicator->IsDrawingAttention());

  // Draw attention for an overflow-on-overflow panel.
  // Expect the overflow indicator is showing attention.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, *P8, P9, P10, P11)
  EXPECT_FALSE(panels[8]->IsDrawingAttention());
  panels[8]->FlashFrame(true);
  EXPECT_TRUE(panels[8]->IsDrawingAttention());
  EXPECT_TRUE(overflow_indicator->IsDrawingAttention());

  // Draw attention for another overflow-on-overflow panel.
  // Expect the overflow indicator is still showing attention.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, *P8, P9, *P10, P11)
  EXPECT_FALSE(panels[10]->IsDrawingAttention());
  panels[10]->FlashFrame(true);
  EXPECT_TRUE(panels[10]->IsDrawingAttention());
  EXPECT_TRUE(overflow_indicator->IsDrawingAttention());

  // Stop drawing attention for one overflow-on-overflow panel.
  // Expect the overflow indicator is still showing attention.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, P8, P9, *P10, P11)
  panels[8]->FlashFrame(false);
  EXPECT_FALSE(panels[8]->IsDrawingAttention());
  EXPECT_TRUE(overflow_indicator->IsDrawingAttention());

  // Stop drawing attention for the other overflow-on-overflow panel.
  // Expect the overflow indicator is not showing attention.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, P6, (P7, P8, P9, P10, P11)
  panels[10]->FlashFrame(false);
  EXPECT_FALSE(panels[10]->IsDrawingAttention());
  EXPECT_FALSE(overflow_indicator->IsDrawingAttention());

  // Draw attention for the top overflow panel.
  // Expect no impact to the overflow indicator.
  //   docked:               P0, P1, P2, P3
  //   overflow:             P4, P5, *P6, (P7, P8, P9, P10, P11)
  EXPECT_TRUE(IsPanelVisible(panels[6]));
  EXPECT_FALSE(panels[6]->IsDrawingAttention());
  panels[6]->FlashFrame(true);
  EXPECT_TRUE(panels[6]->IsDrawingAttention());
  EXPECT_FALSE(overflow_indicator->IsDrawingAttention());

  // Widen a panel in the dock to bump a panel to the overflow area
  // and also get the top visible overflow panel bumped to overflow-on-overflow.
  // Expect the overflow indicator is showing attention.
  //   docked:               P0, P1, P2,
  //   overflow:             P3, P4, P5, (*P6, P7, P8, P9, P10, P11)
  gfx::Size larger_size = panels[0]->GetBounds().size();
  larger_size.Enlarge(150, 50);
  panel_manager->ResizePanel(panels[0], larger_size);
  WaitForLayoutModeChanged(panels[3], PanelStrip::IN_OVERFLOW);
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(9, overflow_strip->num_panels());
  WaitForBoundsAnimationFinished(panels[6]);
  EXPECT_FALSE(IsPanelVisible(panels[6]));
  EXPECT_TRUE(panels[6]->IsDrawingAttention());
  EXPECT_TRUE(overflow_indicator->IsDrawingAttention());

  // Close an overflow panel that would cause the first oveflow-on-overflow
  // panel to become visible. Expect the overflow indicator is no longer
  // showing attention.
  //   docked:               P0, P1, P2,
  //   overflow:             P4, P5, *P6, (P7, P8, P9, P10, P11)
  CloseWindowAndWait(panels[3]->browser());
  EXPECT_EQ(3, docked_strip->num_panels());
  EXPECT_EQ(8, overflow_strip->num_panels());
  EXPECT_TRUE(panels[6]->IsDrawingAttention());
  EXPECT_FALSE(overflow_indicator->IsDrawingAttention());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelOverflowBrowserTest, FullScreenMode) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();
  int iconified_width = overflow_strip->current_display_width();

  // Create docked and overflow panels.
  //   docked:               P0, P1, P2
  //   overflow:             P3, P4
  const int panel_widths[] = {
      250, 260, 200,  // docked
      255, 220        // overflow
  };
  std::vector<Panel*> panels = CreateOverflowPanels(3, 2, panel_widths);

  // Mouse over overflow panel expands overflow area.
  gfx::Point in_overflow_area = panels[4]->GetBounds().origin();
  EXPECT_TRUE(panel_manager->mouse_watcher()->IsActive());
  MoveMouse(in_overflow_area);
  EXPECT_GT(overflow_strip->current_display_width(), iconified_width);
  gfx::Point beyond_overflow_area =
      gfx::Point(overflow_strip->current_display_width() + 5, 10);
  MoveMouse(beyond_overflow_area);
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // In full screen mode, no expansion.
  overflow_strip->OnFullScreenModeChanged(true);
  EXPECT_FALSE(panel_manager->mouse_watcher()->IsActive());
  MoveMouse(in_overflow_area);
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  // End full screen mode, expansion occurs again.
  overflow_strip->OnFullScreenModeChanged(false);
  EXPECT_TRUE(panel_manager->mouse_watcher()->IsActive());
  MoveMouse(in_overflow_area);
  EXPECT_GT(overflow_strip->current_display_width(), iconified_width);
  MoveMouse(beyond_overflow_area);
  EXPECT_EQ(iconified_width, overflow_strip->current_display_width());

  panel_manager->CloseAll();
}
