// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/test/testing_profile.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingAutocompleteEditView : public AutocompleteEditView {
 public:
  TestingAutocompleteEditView() {}

  virtual AutocompleteEditModel* model() { return NULL; }
  virtual const AutocompleteEditModel* model() const { return NULL; }
  virtual void SaveStateToTab(TabContents* tab) {}
  virtual void Update(const TabContents* tab_for_state_restoring) {}
  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword) {}
  virtual std::wstring GetText() const { return std::wstring(); }
  virtual bool IsEditingOrEmpty() const { return true; }
  virtual int GetIcon() const { return 0; }
  virtual void SetUserText(const std::wstring& text) {}
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup) {}
  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos) {}
  virtual void SetForcedQuery() {}
  virtual bool IsSelectAll() { return false; }
  virtual bool DeleteAtEndPressed() { return false; }
  virtual void GetSelectionBounds(std::wstring::size_type* start,
                                  std::wstring::size_type* end) {}
  virtual void SelectAll(bool reversed) {}
  virtual void RevertAll() {}
  virtual void UpdatePopup() {}
  virtual void ClosePopup() {}
  virtual void SetFocus() {}
  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection) {}
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length) {
    return false;
  }
  virtual void OnRevertTemporaryText() {}
  virtual void OnBeforePossibleChange() {}
  virtual bool OnAfterPossibleChange() { return false; }
  virtual gfx::NativeView GetNativeView() const { return 0; }
  virtual CommandUpdater* GetCommandUpdater() { return NULL; }
  virtual void SetInstantSuggestion(const string16& input) {}
  virtual int TextWidth() const { return 0; }
  virtual bool IsImeComposing() const { return false; }

#if defined(TOOLKIT_VIEWS)
  virtual views::View* AddToView(views::View* parent) { return NULL; }
  virtual bool CommitInstantSuggestion(
      const std::wstring& typed_text,
      const std::wstring& suggested_text) { return false;}
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAutocompleteEditView);
};

class TestingAutocompleteEditController : public AutocompleteEditController {
 public:
  TestingAutocompleteEditController() {}
  virtual void OnAutocompleteWillClosePopup() {}
  virtual void OnAutocompleteLosingFocus(gfx::NativeView view_gaining_focus) {}
  virtual void OnAutocompleteWillAccept() {}
  virtual bool OnCommitSuggestedText(const std::wstring& typed_text) {
    return false;
  }
  virtual bool AcceptCurrentInstantPreview() {
    return false;
  }
  virtual void OnPopupBoundsChanged(const gfx::Rect& bounds) {}
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url) {}
  virtual void OnChanged() {}
  virtual void OnSelectionBoundsChanged() {}
  virtual void OnInputInProgress(bool in_progress) {}
  virtual void OnKillFocus() {}
  virtual void OnSetFocus() {}
  virtual SkBitmap GetFavIcon() const { return SkBitmap(); }
  virtual std::wstring GetTitle() const { return std::wstring(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAutocompleteEditController);
};

}

typedef testing::Test AutocompleteEditTest;

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST(AutocompleteEditTest, AdjustTextForCopy) {
  struct Data {
    const wchar_t* perm_text;
    const int sel_start;
    const bool is_all_selected;
    const wchar_t* input;
    const wchar_t* expected_output;
    const bool write_url;
    const char* expected_url;
  } input[] = {
    // Test that http:// is inserted if all text is selected.
    { L"a.b/c", 0, true, L"a.b/c", L"http://a.b/c", true, "http://a.b/c" },

    // Test that http:// is inserted if the host is selected.
    { L"a.b/c", 0, false, L"a.b/", L"http://a.b/", true, "http://a.b/" },

    // Tests that http:// is inserted if the path is modified.
    { L"a.b/c", 0, false, L"a.b/d", L"http://a.b/d", true, "http://a.b/d" },

    // Tests that http:// isn't inserted if the host is modified.
    { L"a.b/c", 0, false, L"a.c/", L"a.c/", false, "" },

    // Tests that http:// isn't inserted if the start of the selection is 1.
    { L"a.b/c", 1, false, L"a.b/", L"a.b/", false, "" },

    // Tests that http:// isn't inserted if a portion of the host is selected.
    { L"a.com/", 0, false, L"a.co", L"a.co", false, "" },

    // Tests that http:// isn't inserted for an https url after the user nukes
    // https.
    { L"https://a.com/", 0, false, L"a.com/", L"a.com/", false, "" },

    // Tests that http:// isn't inserted if the user adds to the host.
    { L"a.b/", 0, false, L"a.bc/", L"a.bc/", false, "" },

    // Tests that we don't get double http if the user manually inserts http.
    { L"a.b/", 0, false, L"http://a.b/", L"http://a.b/", true, "http://a.b/" },
  };
  TestingAutocompleteEditView view;
  TestingAutocompleteEditController controller;
  TestingProfile profile;
  AutocompleteEditModel model(&view, &controller, &profile);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input); ++i) {
    model.UpdatePermanentText(input[i].perm_text);

    std::wstring result(input[i].input);
    GURL url;
    bool write_url;
    model.AdjustTextForCopy(input[i].sel_start, input[i].is_all_selected,
                            &result, &url, &write_url);
    EXPECT_EQ(input[i].expected_output, result) << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}
