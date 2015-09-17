// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_unittest.h"
#include "chrome/common/icon_with_badge_image_source.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// Tests the icon appearance of extension actions without the toolbar redesign.
// In this case, the action should never be grayscaled or decorated to indicate
// whether or not it wants to run.
TEST_F(ToolbarActionsBarUnitTest, ExtensionActionNormalAppearance) {
  CreateAndAddExtension("extension",
                        extensions::extension_action_test_util::BROWSER_ACTION);
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  AddTab(browser(), GURL("chrome://newtab"));

  gfx::Size size(ToolbarActionsBar::IconWidth(false),
                 ToolbarActionsBar::IconHeight());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionViewController* action =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[0]);
  scoped_ptr<IconWithBadgeImageSource> image_source =
      action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, false);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());

  toolbar_model()->SetVisibleIconCount(0u);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, true);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());
}

// Tests the icon appearance of extension actions with the toolbar redesign.
// Extensions that don't want to run should have their icons grayscaled.
// Overflowed extensions that want to run should have an additional decoration.
TEST_F(ToolbarActionsBarRedesignUnitTest, ExtensionActionWantsToRunAppearance) {
  CreateAndAddExtension("extension",
                        extensions::extension_action_test_util::PAGE_ACTION);
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, overflow_bar()->GetIconCount());

  AddTab(browser(), GURL("chrome://newtab"));

  gfx::Size size(ToolbarActionsBar::IconWidth(false),
                 ToolbarActionsBar::IconHeight());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionViewController* action =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[0]);
  scoped_ptr<IconWithBadgeImageSource> image_source =
      action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, true);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());

  toolbar_model()->SetVisibleIconCount(0u);
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(1u, overflow_bar()->GetIconCount());

  action = static_cast<ExtensionActionViewController*>(
      overflow_bar()->GetActions()[0]);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_FALSE(image_source->grayscale());
  EXPECT_TRUE(image_source->paint_decoration());

  SetActionWantsToRunOnTab(action->extension_action(), web_contents, false);
  image_source = action->GetIconImageSourceForTesting(web_contents, size);
  EXPECT_TRUE(image_source->grayscale());
  EXPECT_FALSE(image_source->paint_decoration());
}

TEST_F(ToolbarActionsBarRedesignUnitTest, ExtensionActionContextMenu) {
  CreateAndAddExtension("extension",
                        extensions::extension_action_test_util::BROWSER_ACTION);
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // Check that the context menu has the proper string for the action's position
  // (in the main toolbar, in the overflow container, or temporarily popped
  // out).
  auto check_visibility_string = [](ToolbarActionViewController* action,
                                    int expected_visibility_string) {
    ui::SimpleMenuModel* context_menu =
        static_cast<ui::SimpleMenuModel*>(action->GetContextMenu());
    int visibility_index = context_menu->GetIndexOfCommandId(
        extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
    ASSERT_GE(visibility_index, 0);
    base::string16 visibility_label =
        context_menu->GetLabelAt(visibility_index);
    EXPECT_EQ(l10n_util::GetStringUTF16(expected_visibility_string),
              visibility_label);
  };

  check_visibility_string(toolbar_actions_bar()->GetActions()[0],
                          IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU);
  toolbar_model()->SetVisibleIconCount(0u);
  check_visibility_string(overflow_bar()->GetActions()[0],
                          IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR);
  base::RunLoop run_loop;
  toolbar_actions_bar()->PopOutAction(toolbar_actions_bar()->GetActions()[0],
                                      run_loop.QuitClosure());
  run_loop.Run();
  check_visibility_string(toolbar_actions_bar()->GetActions()[0],
                          IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR);
}
