// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::WebContents;

namespace {

class TestingOmniboxView : public OmniboxView {
 public:
  TestingOmniboxView() {}

  virtual AutocompleteEditModel* model() OVERRIDE { return NULL; }
  virtual const AutocompleteEditModel* model() const OVERRIDE { return NULL; }
  virtual void SaveStateToTab(WebContents* tab) OVERRIDE {}
  virtual void Update(const WebContents* tab_for_state_restoring) OVERRIDE {}
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         size_t selected_line) OVERRIDE {}
  virtual string16 GetText() const OVERRIDE { return string16(); }
  virtual bool IsEditingOrEmpty() const OVERRIDE { return true; }
  virtual int GetIcon() const OVERRIDE { return 0; }
  virtual void SetUserText(const string16& text) OVERRIDE {}
  virtual void SetUserText(const string16& text,
                           const string16& display_text,
                           bool update_popup) OVERRIDE {}
  virtual void SetWindowTextAndCaretPos(const string16& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) OVERRIDE {}
  virtual void SetForcedQuery() OVERRIDE {}
  virtual bool IsSelectAll() const OVERRIDE { return false; }
  virtual bool DeleteAtEndPressed() OVERRIDE { return false; }
  virtual void GetSelectionBounds(size_t* start, size_t* end) const OVERRIDE {}
  virtual void SelectAll(bool reversed) OVERRIDE {}
  virtual void RevertAll() OVERRIDE {}
  virtual void UpdatePopup() OVERRIDE {}
  virtual void ClosePopup() OVERRIDE {}
  virtual void SetFocus() OVERRIDE {}
  virtual void OnTemporaryTextMaybeChanged(
      const string16& display_text,
      bool save_original_selection) OVERRIDE {}
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length) OVERRIDE {
    return false;
  }
  virtual void OnRevertTemporaryText() OVERRIDE {}
  virtual void OnBeforePossibleChange() OVERRIDE {}
  virtual bool OnAfterPossibleChange() OVERRIDE { return false; }
  virtual gfx::NativeView GetNativeView() const OVERRIDE { return NULL; }
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE {
    return NULL;
  }
  virtual CommandUpdater* GetCommandUpdater() OVERRIDE { return NULL; }
  virtual void SetInstantSuggestion(const string16& input,
                                    bool animate_to_complete) OVERRIDE {}
  virtual string16 GetInstantSuggestion() const OVERRIDE { return string16(); }
  virtual int TextWidth() const OVERRIDE { return 0; }
  virtual bool IsImeComposing() const OVERRIDE { return false; }

#if defined(TOOLKIT_VIEWS)
  virtual int GetMaxEditWidth(int entry_width) const OVERRIDE {
    return entry_width;
  }
  virtual views::View* AddToView(views::View* parent) OVERRIDE { return NULL; }
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE {
    return 0;
  }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

class TestingAutocompleteEditController : public AutocompleteEditController {
 public:
  TestingAutocompleteEditController() {}
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    content::PageTransition transition,
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

class AutocompleteEditTest : public ::testing::Test {};

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST_F(AutocompleteEditTest, AdjustTextForCopy) {
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

    // Makes sure intranet urls get 'http://' prefixed to them.
    { "b/foo", 0, true, "b/foo", "http://b/foo", true, "http://b/foo" },

    // Verifies a search term 'foo' doesn't end up with http.
    { "www.google.com/search?", 0, false, "foo", "foo", false, "" },
  };
  TestingOmniboxView view;
  TestingAutocompleteEditController controller;
  TestingProfile profile;
  // NOTE: The TemplateURLService must be created before the
  // AutocompleteClassifier so that the SearchProvider gets a non-NULL
  // TemplateURLService at construction time.
  profile.CreateTemplateURLService();
  profile.CreateAutocompleteClassifier();
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
