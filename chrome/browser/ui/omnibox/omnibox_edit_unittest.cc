// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/test_toolbar_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

class TestingOmniboxView : public OmniboxView {
 public:
  explicit TestingOmniboxView(OmniboxEditController* controller)
      : OmniboxView(NULL, controller, NULL) {}

  virtual void SaveStateToTab(WebContents* tab) OVERRIDE {}
  virtual void OnTabChanged(const WebContents* web_contents) OVERRIDE {}
  virtual void Update() OVERRIDE {}
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         size_t selected_line) OVERRIDE {}
  virtual string16 GetText() const OVERRIDE { return string16(); }
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
  virtual void SetFocus() OVERRIDE {}
  virtual void ApplyCaretVisibility() OVERRIDE {}
  virtual void OnTemporaryTextMaybeChanged(
      const string16& display_text,
      bool save_original_selection,
      bool notify_text_changed) OVERRIDE {}
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
  virtual void SetGrayTextAutocompletion(const string16& input) OVERRIDE {}
  virtual string16 GetGrayTextAutocompletion() const OVERRIDE {
    return string16();
  }
  virtual int TextWidth() const OVERRIDE { return 0; }
  virtual bool IsImeComposing() const OVERRIDE { return false; }

#if defined(TOOLKIT_VIEWS)
  virtual int GetMaxEditWidth(int entry_width) const OVERRIDE {
    return entry_width;
  }
  virtual views::View* AddToView(views::View* parent) OVERRIDE { return NULL; }
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE {
    return 0;
  }
#endif

  virtual int GetOmniboxTextLength() const OVERRIDE { return 0; }
  virtual void EmphasizeURLComponents() OVERRIDE { }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

class TestingOmniboxEditController : public OmniboxEditController {
 public:
  explicit TestingOmniboxEditController(ToolbarModel* toolbar_model)
      : OmniboxEditController(NULL),
        toolbar_model_(toolbar_model) {
  }
  virtual void Update(const content::WebContents* contents) OVERRIDE {}
  virtual void OnChanged() OVERRIDE {}
  virtual void OnSetFocus() OVERRIDE {}
  virtual InstantController* GetInstant() OVERRIDE { return NULL; }
  virtual WebContents* GetWebContents() OVERRIDE { return NULL; }
  virtual ToolbarModel* GetToolbarModel() OVERRIDE { return toolbar_model_; }
  virtual const ToolbarModel* GetToolbarModel() const OVERRIDE {
    return toolbar_model_;
  }

 private:
  ToolbarModel* toolbar_model_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

class AutocompleteEditTest : public ::testing::Test {
 public:
   TestToolbarModel* toolbar_model() { return &toolbar_model_; }

  private:
   TestToolbarModel toolbar_model_;
};

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
    const bool extracted_search_terms;
  } input[] = {
    // Test that http:// is inserted if all text is selected.
    { "a.de/b", 0, true, "a.de/b", "http://a.de/b", true, "http://a.de/b",
      false },

    // Test that http:// is inserted if the host is selected.
    { "a.de/b", 0, false, "a.de/", "http://a.de/", true, "http://a.de/",
      false },

    // Tests that http:// is inserted if the path is modified.
    { "a.de/b", 0, false, "a.de/c", "http://a.de/c", true, "http://a.de/c",
      false },

    // Tests that http:// isn't inserted if the host is modified.
    { "a.de/b", 0, false, "a.com/b", "a.com/b", false, "", false },

    // Tests that http:// isn't inserted if the start of the selection is 1.
    { "a.de/b", 1, false, "a.de/b", "a.de/b", false, "", false },

    // Tests that http:// isn't inserted if a portion of the host is selected.
    { "a.de/", 0, false, "a.d", "a.d", false, "", false },

    // Tests that http:// isn't inserted for an https url after the user nukes
    // https.
    { "https://a.com/", 0, false, "a.com/", "a.com/", false, "", false },

    // Tests that http:// isn't inserted if the user adds to the host.
    { "a.de/", 0, false, "a.de.com/", "a.de.com/", false, "", false },

    // Tests that we don't get double http if the user manually inserts http.
    { "a.de/", 0, false, "http://a.de/", "http://a.de/", true, "http://a.de/",
      false },

    // Makes sure intranet urls get 'http://' prefixed to them.
    { "b/foo", 0, true, "b/foo", "http://b/foo", true, "http://b/foo", false },

    // Verifies a search term 'foo' doesn't end up with http.
    { "www.google.com/search?", 0, false, "foo", "foo", false, "", false },

    // Makes sure extracted search terms are not modified.
    { "www.google.com/webhp?", 0, true, "hello world", "hello world", false,
      "", true },
  };
  TestingOmniboxEditController controller(toolbar_model());
  TestingOmniboxView view(&controller);
  TestingProfile profile;
  // NOTE: The TemplateURLService must be created before the
  // AutocompleteClassifier so that the SearchProvider gets a non-NULL
  // TemplateURLService at construction time.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile, &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile, &AutocompleteClassifierFactory::BuildInstanceFor);
  OmniboxEditModel model(&view, &controller, &profile);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input); ++i) {
    toolbar_model()->set_text(ASCIIToUTF16(input[i].perm_text));
    model.UpdatePermanentText();

    toolbar_model()->set_replace_search_url_with_search_terms(
        input[i].extracted_search_terms);

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
