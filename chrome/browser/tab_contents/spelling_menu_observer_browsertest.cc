// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spelling_menu_observer.h"

#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"

using content::RenderViewHost;
using content::WebContents;

namespace {

// A mock context menu used in this test. This class overrides virtual methods
// derived from the RenderViewContextMenuProxy class to monitor calls from the
// SpellingMenuObserver class.
class MockRenderViewContextMenu : public RenderViewContextMenuProxy {
 public:
  // A menu item used in this test. This test uses a vector of this struct to
  // hold menu items added by this test.
  struct MockMenuItem {
    MockMenuItem()
        : command_id(0),
          enabled(false),
          checked(false),
          hidden(true) {
    }
    int command_id;
    bool enabled;
    bool checked;
    bool hidden;
    string16 title;
  };

  MockRenderViewContextMenu();
  virtual ~MockRenderViewContextMenu();

  // RenderViewContextMenuProxy implementation.
  virtual void AddMenuItem(int command_id, const string16& title) OVERRIDE;
  virtual void AddCheckItem(int command_id, const string16& title) OVERRIDE;
  virtual void AddSeparator() OVERRIDE;
  virtual void AddSubMenu(int command_id,
                          const string16& label,
                          ui::MenuModel* model) OVERRIDE;
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual Profile* GetProfile() const OVERRIDE;

  // Create a testing URL request context.
  void CreateRequestContext() {
    profile_->CreateRequestContext();
  }

  // Attaches a RenderViewContextMenuObserver to be tested.
  void SetObserver(RenderViewContextMenuObserver* observer);

  // Returns the number of items added by the test.
  size_t GetMenuSize() const;

  // Returns the i-th item.
  bool GetMenuItem(size_t i, MockMenuItem* item) const;

  // Returns the writable profile used in this test.
  PrefService* GetPrefs();

 private:
  // An observer used for initializing the status of menu items added in this
  // test. A test should delete this RenderViewContextMenuObserver object.
  RenderViewContextMenuObserver* observer_;

  // A dummy profile used in this test. Call GetPrefs() when a test needs to
  // change this profile and use PrefService methods.
  scoped_ptr<TestingProfile> profile_;

  // A list of menu items added by the SpellingMenuObserver class.
  std::vector<MockMenuItem> items_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewContextMenu);
};

MockRenderViewContextMenu::MockRenderViewContextMenu()
  : observer_(NULL),
    profile_(new TestingProfile) {
}

MockRenderViewContextMenu::~MockRenderViewContextMenu() {
}

void MockRenderViewContextMenu::AddMenuItem(int command_id,
                                            const string16& title) {
  MockMenuItem item;
  item.command_id = command_id;
  item.enabled = observer_->IsCommandIdEnabled(command_id);
  item.checked = false;
  item.hidden = false;
  item.title = title;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddCheckItem(int command_id,
                                             const string16& title) {
  MockMenuItem item;
  item.command_id = command_id;
  item.enabled = observer_->IsCommandIdEnabled(command_id);
  item.checked = observer_->IsCommandIdChecked(command_id);
  item.hidden = false;
  item.title = title;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddSeparator() {
  MockMenuItem item;
  item.command_id = -1;
  item.enabled = false;
  item.checked = false;
  item.hidden = false;
  items_.push_back(item);
}

void MockRenderViewContextMenu::AddSubMenu(int command_id,
                                           const string16& label,
                                           ui::MenuModel* model) {
  MockMenuItem item;
  item.command_id = -1;
  item.enabled = false;
  item.checked = false;
  item.hidden = false;
  items_.push_back(item);
}

void MockRenderViewContextMenu::UpdateMenuItem(int command_id,
                                               bool enabled,
                                               bool hidden,
                                               const string16& title) {
  for (std::vector<MockMenuItem>::iterator it = items_.begin();
       it != items_.end(); ++it) {
    if (it->command_id == command_id) {
      it->enabled = enabled;
      it->hidden = hidden;
      it->title = title;
      return;
    }
  }

  // The SpellingMenuObserver class tries to change a menu item not added by the
  // class. This is an unexpected behavior and we should stop now.
  FAIL();
}

RenderViewHost* MockRenderViewContextMenu::GetRenderViewHost() const {
  return NULL;
}

WebContents* MockRenderViewContextMenu::GetWebContents() const {
  return NULL;
}

Profile* MockRenderViewContextMenu::GetProfile() const {
  return profile_.get();
}

size_t MockRenderViewContextMenu::GetMenuSize() const {
  return items_.size();
}

bool MockRenderViewContextMenu::GetMenuItem(size_t i,
                                            MockMenuItem* item) const {
  if (i >= items_.size())
    return false;
  item->command_id = items_[i].command_id;
  item->enabled = items_[i].enabled;
  item->checked = items_[i].checked;
  item->hidden = items_[i].hidden;
  item->title = items_[i].title;
  return true;
}

void MockRenderViewContextMenu::SetObserver(
    RenderViewContextMenuObserver* observer) {
  observer_ = observer;
}

PrefService* MockRenderViewContextMenu::GetPrefs() {
  return profile_->GetPrefs();
}

// A test class used in this file. This test should be a browser test because it
// accesses resources.
class SpellingMenuObserverTest : public InProcessBrowserTest {
 public:
  SpellingMenuObserverTest();
  virtual ~SpellingMenuObserverTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(SpellingMenuObserverTest);
};

SpellingMenuObserverTest::SpellingMenuObserverTest() {
}

SpellingMenuObserverTest::~SpellingMenuObserverTest() {
}

}  // namespace

// Tests that right-clicking a correct word does not add any items.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithCorrectWord) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());

  content::ContextMenuParams params;
  params.is_editable = true;
  observer->InitMenu(params);
  EXPECT_EQ(static_cast<size_t>(0), menu->GetMenuSize());
}

// Tests that right-clicking a misspelled word adds four items:
// "No spelling suggestions", "Add to dictionary", "Ask Google for suggesitons",
// and a separator.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, InitMenuWithMisspelledWord) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());

  content::ContextMenuParams params;
  params.is_editable = true;
  params.misspelled_word = ASCIIToUTF16("wiimode");
  observer->InitMenu(params);
  EXPECT_EQ(static_cast<size_t>(4), menu->GetMenuSize());

  // Read all the context-menu items added by this test and verify they are
  // expected ones. We do not check the item titles to prevent resource changes
  // from breaking this test. (I think it is not expected by those who change
  // resources.)
  MockRenderViewContextMenu::MockMenuItem item;
  menu->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
  menu->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_SPELLCHECK_ADD_TO_DICTIONARY, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  menu->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  menu->GetMenuItem(3, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Tests that right-clicking a correct word when we enable spelling-service
// integration to verify an item "Ask Google for suggestions" is checked. Even
// though this meanu itself does not add this item, its sub-menu adds the item
// and calls SpellingMenuObserver::IsChecked() to check it.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       EnableSpellingServiceWithCorrectWord) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());
  menu->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  content::ContextMenuParams params;
  params.is_editable = true;
  observer->InitMenu(params);

  EXPECT_TRUE(
      observer->IsCommandIdChecked(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE));
}

// Tests that right-clicking a misspelled word when we enable spelling-service
// integration to verify an item "Ask Google for suggestions" is checked. (This
// test does not actually send JSON-RPC requests to the service because it makes
// this test flaky.)
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, EnableSpellingService) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());
  menu->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
  menu->GetPrefs()->SetString(prefs::kSpellCheckDictionary, "");

  content::ContextMenuParams params;
  params.is_editable = true;
  params.misspelled_word = ASCIIToUTF16("wiimode");
  observer->InitMenu(params);
  EXPECT_EQ(static_cast<size_t>(4), menu->GetMenuSize());

  // To avoid duplicates, this test reads only the "Ask Google for suggestions"
  // item and verifies it is enabled and checked.
  MockRenderViewContextMenu::MockMenuItem item;
  menu->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}

// Test that there will be a separator after "no suggestions" if
// SpellingServiceClient::SUGGEST is on.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest, SeparatorAfterSuggestions) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());
  menu->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  // Make sure we can pretend to handle the JSON request.
  menu->CreateRequestContext();

  // Force a non-empty locale so SUGGEST is available.
  menu->GetPrefs()->SetString(prefs::kSpellCheckDictionary, "en");
  EXPECT_TRUE(SpellingServiceClient::IsAvailable(menu->GetProfile(),
    SpellingServiceClient::SUGGEST));

  content::ContextMenuParams params;
  params.is_editable = true;
  params.misspelled_word = ASCIIToUTF16("jhhj");
  observer->InitMenu(params);

  // The test should see "No spelling suggestions" (from system checker),
  // "No more Google suggestions" (from SpellingService) and a separator
  // as the first three items, then possibly more (not relevant here).
  EXPECT_LT(3U, menu->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu->GetMenuItem(2, &item);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}

// Test that we don't show "No more suggestions from Google" if the spelling
// service is enabled and that there is only one suggestion.
IN_PROC_BROWSER_TEST_F(SpellingMenuObserverTest,
                       NoMoreSuggestionsNotDisplayed) {
  scoped_ptr<MockRenderViewContextMenu> menu(new MockRenderViewContextMenu);
  scoped_ptr<SpellingMenuObserver> observer(
      new SpellingMenuObserver(menu.get()));
  menu->SetObserver(observer.get());
  menu->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kUseSpellingService);

  // Make sure we can pretend to handle the JSON request.
  menu->CreateRequestContext();

  // Force a non-empty locale so SPELLCHECK is available.
  menu->GetPrefs()->SetString(prefs::kSpellCheckDictionary, "en");
  EXPECT_TRUE(SpellingServiceClient::IsAvailable(menu->GetProfile(),
    SpellingServiceClient::SPELLCHECK));

  content::ContextMenuParams params;
  params.is_editable = true;
  params.misspelled_word = ASCIIToUTF16("asdfkj");
  params.dictionary_suggestions.push_back(ASCIIToUTF16("asdf"));
  observer->InitMenu(params);

  // The test should see a suggestion (from SpellingService) and a separator
  // as the first two items, then possibly more (not relevant here).
  EXPECT_LT(2U, menu->GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  menu->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_SPELLCHECK_SUGGESTION_0, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);

  menu->GetMenuItem(1, &item);
  EXPECT_NE(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS, item.command_id);
  EXPECT_EQ(-1, item.command_id);
  EXPECT_FALSE(item.enabled);
  EXPECT_FALSE(item.hidden);
}
