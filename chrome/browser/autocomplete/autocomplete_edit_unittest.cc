// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

class TestingOmniboxView : public OmniboxView {
 public:
  TestingOmniboxView() {}

  virtual AutocompleteEditModel* model() { return NULL; }
  virtual const AutocompleteEditModel* model() const { return NULL; }
  virtual void SaveStateToTab(TabContents* tab) {}
  virtual void Update(const TabContents* tab_for_state_restoring) {}
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         size_t selected_line,
                         const string16& keyword) {}
  virtual string16 GetText() const { return string16(); }
  virtual bool IsEditingOrEmpty() const { return true; }
  virtual int GetIcon() const { return 0; }
  virtual void SetUserText(const string16& text) {}
  virtual void SetUserText(const string16& text,
                           const string16& display_text,
                           bool update_popup) {}
  virtual void SetWindowTextAndCaretPos(const string16& text,
                                        size_t caret_pos) {}
  virtual void SetForcedQuery() {}
  virtual bool IsSelectAll() { return false; }
  virtual bool DeleteAtEndPressed() { return false; }
  virtual void GetSelectionBounds(size_t* start, size_t* end) {}
  virtual void SelectAll(bool reversed) {}
  virtual void RevertAll() {}
  virtual void UpdatePopup() {}
  virtual void ClosePopup() {}
  virtual void SetFocus() {}
  virtual void OnTemporaryTextMaybeChanged(const string16& display_text,
                                           bool save_original_selection) {}
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length) {
    return false;
  }
  virtual void OnRevertTemporaryText() {}
  virtual void OnBeforePossibleChange() {}
  virtual bool OnAfterPossibleChange() { return false; }
  virtual gfx::NativeView GetNativeView() const { return 0; }
  virtual CommandUpdater* GetCommandUpdater() { return NULL; }
  virtual void SetInstantSuggestion(const string16& input,
                                    bool animate_to_complete) {}
  virtual string16 GetInstantSuggestion() const { return string16(); }
  virtual int TextWidth() const { return 0; }
  virtual bool IsImeComposing() const { return false; }

#if defined(TOOLKIT_VIEWS)
  virtual views::View* AddToView(views::View* parent) { return NULL; }
  virtual int OnPerformDrop(const views::DropTargetEvent& event) { return 0; }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

class TestingAutocompleteEditController : public AutocompleteEditController {
 public:
  TestingAutocompleteEditController() {}
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url) OVERRIDE {}
  virtual void OnChanged() OVERRIDE {}
  virtual void OnSelectionBoundsChanged() OVERRIDE {}
  virtual void OnInputInProgress(bool in_progress) OVERRIDE {}
  virtual void OnKillFocus() OVERRIDE {}
  virtual void OnSetFocus() OVERRIDE {}
  virtual SkBitmap GetFavicon() const OVERRIDE { return SkBitmap(); }
  virtual string16 GetTitle() const OVERRIDE { return string16(); }
  virtual InstantController* GetInstant() OVERRIDE { return NULL; }
  virtual TabContentsWrapper* GetTabContentsWrapper() const OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAutocompleteEditController);
};

}  // namespace

typedef testing::Test AutocompleteEditTest;

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST(AutocompleteEditTest, AdjustTextForCopy) {
  struct Data {
    const char* perm_text;
    const int sel_start;
    const bool is_all_selected;
    const char* input;
    const char* expected_output;
    const bool write_url;
    const char* expected_url;
  } input[] = {
    // Test that http:// is inserted if all text is selected.
    { "a.de/b", 0, true, "a.de/b", "http://a.de/b", true, "http://a.de/b" },

    // Test that http:// is inserted if the host is selected.
    { "a.de/b", 0, false, "a.de/", "http://a.de/", true, "http://a.de/" },

    // Tests that http:// is inserted if the path is modified.
    { "a.de/b", 0, false, "a.de/c", "http://a.de/c", true, "http://a.de/c" },

    // Tests that http:// isn't inserted if the host is modified.
    { "a.de/b", 0, false, "a.com/b", "a.com/b", false, "" },

    // Tests that http:// isn't inserted if the start of the selection is 1.
    { "a.de/b", 1, false, "a.de/b", "a.de/b", false, "" },

    // Tests that http:// isn't inserted if a portion of the host is selected.
    { "a.de/", 0, false, "a.d", "a.d", false, "" },

    // Tests that http:// isn't inserted for an https url after the user nukes
    // https.
    { "https://a.com/", 0, false, "a.com/", "a.com/", false, "" },

    // Tests that http:// isn't inserted if the user adds to the host.
    { "a.de/", 0, false, "a.de.com/", "a.de.com/", false, "" },

    // Tests that we don't get double http if the user manually inserts http.
    { "a.de/", 0, false, "http://a.de/", "http://a.de/", true, "http://a.de/" },
  };
  TestingOmniboxView view;
  TestingAutocompleteEditController controller;
  TestingProfile profile;
  AutocompleteEditModel model(&view, &controller, &profile);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input); ++i) {
    model.UpdatePermanentText(ASCIIToUTF16(input[i].perm_text));

    string16 result = ASCIIToUTF16(input[i].input);
    GURL url;
    bool write_url;
    model.AdjustTextForCopy(input[i].sel_start, input[i].is_all_selected,
                            &result, &url, &write_url);
    EXPECT_EQ(ASCIIToUTF16(input[i].expected_output), result) << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}
