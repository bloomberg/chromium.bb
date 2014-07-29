// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spellchecker_submenu_observer.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

using content::RenderViewHost;
using content::WebContents;

namespace {

// A mock context menu used in this test. This class overrides virtual methods
// derived from the RenderViewContextMenuProxy class to monitor calls from the
// SpellingMenuObserver class.
class MockRenderViewContextMenu : public ui::SimpleMenuModel::Delegate,
                                  public RenderViewContextMenuProxy {
 public:
  // A menu item used in this test.
  struct MockMenuItem {
    MockMenuItem()
        : command_id(0),
          enabled(false),
          checked(false),
          hidden(true) {}
    int command_id;
    bool enabled;
    bool checked;
    bool hidden;
    base::string16 title;
  };

  MockRenderViewContextMenu() : observer_(NULL), profile_(new TestingProfile) {}
  virtual ~MockRenderViewContextMenu() {}

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return observer_->IsCommandIdChecked(command_id);
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return observer_->IsCommandIdEnabled(command_id);
  }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    observer_->ExecuteCommand(command_id);
  }
  virtual void MenuWillShow(ui::SimpleMenuModel* source) OVERRIDE {}
  virtual void MenuClosed(ui::SimpleMenuModel* source) OVERRIDE {}
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  // RenderViewContextMenuProxy implementation.
  virtual void AddMenuItem(int command_id,
                           const base::string16& title) OVERRIDE {}
  virtual void AddCheckItem(int command_id,
                            const base::string16& title) OVERRIDE {}
  virtual void AddSeparator() OVERRIDE {}
  virtual void AddSubMenu(int command_id,
                          const base::string16& label,
                          ui::MenuModel* model) OVERRIDE {}
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const base::string16& title) OVERRIDE {}
  virtual RenderViewHost* GetRenderViewHost() const OVERRIDE {
    return NULL;
  }
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return profile_.get();
  }
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

  // Attaches a RenderViewContextMenuObserver to be tested.
  void SetObserver(RenderViewContextMenuObserver* observer) {
    observer_ = observer;
  }

  // Returns the number of items added by the test.
  size_t GetMenuSize() const {
    return 0;
  }

  // Returns the i-th item.
  bool GetMenuItem(size_t i, MockMenuItem* item) const {
    return false;
  }

  // Returns the writable profile used in this test.
  PrefService* GetPrefs() {
    return profile_->GetPrefs();
  }

 private:
  // An observer used for initializing the status of menu items added in this
  // test. This is a weak pointer, the test is responsible for deleting this
  // object.
  RenderViewContextMenuObserver* observer_;

  // A dummy profile used in this test. Call GetPrefs() when a test needs to
  // change this profile and use PrefService methods.
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewContextMenu);
};

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellCheckerSubMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellCheckerSubMenuObserverTest() {}
  virtual ~SpellCheckerSubMenuObserverTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SpellCheckerSubMenuObserverTest);
};

}  // namespace

// Tests that selecting the "Check Spelling While Typing" item toggles the value
// of the "browser.enable_spellchecking" profile.
IN_PROC_BROWSER_TEST_F(SpellCheckerSubMenuObserverTest, ToggleSpelling) {
  // Initialize a menu consisting only of a "Spell-checker Options" submenu.
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellCheckerSubMenuObserver> observer(
      new SpellCheckerSubMenuObserver(menu.get(), menu.get(), 1));
  menu->SetObserver(observer.get());
  menu->GetPrefs()->SetString(prefs::kAcceptLanguages, "en-US");
  menu->GetPrefs()->SetString(prefs::kSpellCheckDictionary, "en-US");
  menu->GetPrefs()->SetBoolean(prefs::kEnableContinuousSpellcheck, true);
  content::ContextMenuParams params;
  observer->InitMenu(params);

  // Verify this menu has the "Check Spelling While Typing" item and this item
  // is checked.
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CHECK_SPELLING_WHILE_TYPING));
  EXPECT_TRUE(menu->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));

  // Select this item and verify that the "Check Spelling While Typing" item is
  // not checked. Also, verify that the value of "browser.enable_spellchecking"
  // is now false.
  menu->ExecuteCommand(IDC_CHECK_SPELLING_WHILE_TYPING, 0);
  EXPECT_FALSE(
      menu->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));
  EXPECT_FALSE(menu->IsCommandIdChecked(IDC_CHECK_SPELLING_WHILE_TYPING));
}
