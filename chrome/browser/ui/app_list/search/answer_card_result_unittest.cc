// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card_result.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/views/view.h"

namespace app_list {
namespace test {

namespace {

constexpr char kResultUrl[] = "http://google.com/search?q=weather";
constexpr char kResultTitle[] = "The weather is fine";

}  // namespace

class AnswerCardResultTest : public AppListTestBase,
                             public app_list::SearchResultObserver {
 public:
  AnswerCardResultTest() {}

  void DeleteWebContents() { web_contents_.reset(nullptr); }

  std::unique_ptr<AnswerCardResult> CreateResult(
      const std::string& result_url,
      const base::string16& result_title) const {
    return base::MakeUnique<AnswerCardResult>(
        profile_.get(), app_list_controller_delegate_.get(), result_url,
        result_title, view_.get(), web_contents_.get());
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

  void VerifyLastMouseHoverEvent(bool expected_mouse_in_view,
                                 SearchResult* result) {
    ASSERT_TRUE(received_hover_event_);
    EXPECT_EQ(expected_mouse_in_view, result->is_mouse_in_view());
    received_hover_event_ = false;
  }

  void InjectMouseEvent(blink::WebInputEvent::Type type) {
    web_contents_->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        blink::WebMouseEvent(type, blink::WebInputEvent::kNoModifiers,
                             blink::WebInputEvent::kTimeStampForTesting));
  }

  views::View* view() const { return view_.get(); }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    web_contents_.reset(
        content::WebContents::Create(content::WebContents::CreateParams(
            profile_.get(), content::SiteInstance::Create(profile_.get()))));
    view_ = base::MakeUnique<views::View>();
    app_list_controller_delegate_ =
        base::MakeUnique<::test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    ASSERT_FALSE(received_hover_event_);
    AppListTestBase::TearDown();
  }

  // SearchResultObserver overrides:
  void OnViewHoverStateChanged() override {
    ASSERT_FALSE(received_hover_event_);
    received_hover_event_ = true;
  }

 private:
  std::unique_ptr<views::View> view_;
  std::unique_ptr<content::WebContents> web_contents_;
  bool received_hover_event_ = false;
  std::unique_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardResultTest);
};

TEST_F(AnswerCardResultTest, Basic) {
  std::unique_ptr<AnswerCardResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));

  EXPECT_EQ(kResultUrl, result->id());
  EXPECT_EQ(base::ASCIIToUTF16(kResultTitle), result->title());
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result->display_type());
  EXPECT_EQ(1, result->relevance());
  EXPECT_EQ(view(), result->view());

  result->Open(ui::EF_NONE);
  EXPECT_EQ(kResultUrl, GetLastOpenedUrl().spec());

  std::unique_ptr<SearchResult> result1 = result->Duplicate();

  EXPECT_EQ(kResultUrl, result1->id());
  EXPECT_EQ(base::ASCIIToUTF16(kResultTitle), result1->title());
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result1->display_type());
  EXPECT_EQ(1, result1->relevance());
  EXPECT_EQ(view(), result1->view());
}

TEST_F(AnswerCardResultTest, NullWebContents) {
  DeleteWebContents();

  // Shouldn't crash with null WebContents.
  std::unique_ptr<AnswerCardResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));
  std::unique_ptr<SearchResult> result1 = result->Duplicate();
}

TEST_F(AnswerCardResultTest, EarlyDeleteWebContents) {
  // Shouldn't crash with WebContents gets deleted while search result exists.
  std::unique_ptr<AnswerCardResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));

  DeleteWebContents();

  result->Duplicate();
}

TEST_F(AnswerCardResultTest, MouseEvents) {
  std::unique_ptr<SearchResult> result;
  {
    // Duplicate the result, so that we know that the copy still generates
    // events.
    std::unique_ptr<AnswerCardResult> result_original =
        CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));
    result = result_original->Duplicate();
  }

  result->AddObserver(this);

  ASSERT_FALSE(result->is_mouse_in_view());

  InjectMouseEvent(blink::WebInputEvent::kMouseLeave);
  ASSERT_FALSE(result->is_mouse_in_view());

  InjectMouseEvent(blink::WebInputEvent::kMouseEnter);
  VerifyLastMouseHoverEvent(true, result.get());

  InjectMouseEvent(blink::WebInputEvent::kMouseEnter);
  // Not using VerifyLastMouseHoverEvent will effectively check that second
  // Enter event didn't invoke a callback.
  ASSERT_TRUE(result->is_mouse_in_view());

  InjectMouseEvent(blink::WebInputEvent::kMouseLeave);
  VerifyLastMouseHoverEvent(false, result.get());

  InjectMouseEvent(blink::WebInputEvent::kMouseLeave);
  // Not using VerifyLastMouseHoverEvent will effectively check that second
  // Leave event didn't invoke a callback.
  ASSERT_FALSE(result->is_mouse_in_view());

  InjectMouseEvent(blink::WebInputEvent::kMouseMove);
  VerifyLastMouseHoverEvent(true, result.get());
}

}  // namespace test
}  // namespace app_list
