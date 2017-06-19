// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_contents.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/views/view.h"

namespace app_list {
namespace test {

namespace {

constexpr char kResultUrl[] = "http://google.com/search?q=weather";
constexpr char kResultTitle[] = "The weather is fine";

class AnswerCardTestContents : public AnswerCardContents {
 public:
  AnswerCardTestContents() {}

  // AnswerCardContents overrides:
  void LoadURL(const GURL& url) override { NOTREACHED(); }
  bool IsLoading() const override {
    NOTREACHED();
    return true;
  }
  views::View* GetView() override { return &view_; }

 private:
  views::View view_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardTestContents);
};

}  // namespace

class AnswerCardResultTest : public AppListTestBase,
                             public app_list::SearchResultObserver {
 public:
  AnswerCardResultTest() {}

  void DeleteContents() { contents_.reset(nullptr); }

  std::unique_ptr<AnswerCardResult> CreateResult(
      const std::string& result_url,
      const base::string16& result_title) const {
    return base::MakeUnique<AnswerCardResult>(
        profile_.get(), app_list_controller_delegate_.get(), result_url,
        result_title, contents_.get());
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

  views::View* GetView() const { return contents_->GetView(); }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    contents_ = base::MakeUnique<AnswerCardTestContents>();
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
  std::unique_ptr<AnswerCardTestContents> contents_;
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
  EXPECT_EQ(GetView(), result->view());

  result->Open(ui::EF_NONE);
  EXPECT_EQ(kResultUrl, GetLastOpenedUrl().spec());

  std::unique_ptr<SearchResult> result1 = result->Duplicate();

  EXPECT_EQ(kResultUrl, result1->id());
  EXPECT_EQ(base::ASCIIToUTF16(kResultTitle), result1->title());
  EXPECT_EQ(SearchResult::DISPLAY_CARD, result1->display_type());
  EXPECT_EQ(1, result1->relevance());
  EXPECT_EQ(GetView(), result1->view());
}

TEST_F(AnswerCardResultTest, NullContents) {
  DeleteContents();

  // Shouldn't crash with null contents.
  std::unique_ptr<AnswerCardResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));
  std::unique_ptr<SearchResult> result1 = result->Duplicate();
}

TEST_F(AnswerCardResultTest, EarlyDeleteContents) {
  // Shouldn't crash with contents gets deleted while search result exists.
  std::unique_ptr<AnswerCardResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));

  DeleteContents();

  result->Duplicate();
}

TEST_F(AnswerCardResultTest, MouseEvents) {
  std::unique_ptr<SearchResult> result =
      CreateResult(kResultUrl, base::ASCIIToUTF16(kResultTitle));

  result->AddObserver(this);

  result->SetIsMouseInView(true);
  VerifyLastMouseHoverEvent(true, result.get());

  result->SetIsMouseInView(false);
  VerifyLastMouseHoverEvent(false, result.get());
}

}  // namespace test
}  // namespace app_list
