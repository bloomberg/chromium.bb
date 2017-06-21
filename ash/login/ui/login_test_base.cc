// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_base.h"

#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A WidgetDelegate which ensures that |initially_focused| gets focus.
class LoginTestBase::WidgetDelegate : public views::WidgetDelegate {
 public:
  WidgetDelegate(views::View* content) : content_(content) {}
  ~WidgetDelegate() override = default;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override { return content_; }
  views::Widget* GetWidget() override { return content_->GetWidget(); }
  const views::Widget* GetWidget() const override {
    return content_->GetWidget();
  }

 private:
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDelegate);
};

LoginTestBase::LoginTestBase() {}

LoginTestBase::~LoginTestBase() {}

void LoginTestBase::ShowWidgetWithContent(views::View* content) {
  EXPECT_FALSE(widget_) << "CreateWidget can only be called once.";

  delegate_ = base::MakeUnique<WidgetDelegate>(content);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = CurrentContext();
  params.bounds = gfx::Rect(0, 0, 800, 800);
  params.delegate = delegate_.get();
  widget_ = new views::Widget();
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->Show();
}

mojom::UserInfoPtr LoginTestBase::CreateUser(const std::string& name) const {
  auto user = mojom::UserInfo::New();
  user->account_id = AccountId::FromUserEmail(name + "@foo.com");
  user->display_name = "User " + name;
  user->display_email = user->account_id.GetUserEmail();
  return user;
}

void LoginTestBase::SetUserCount(size_t count) {
  // Add missing users, then remove extra users.
  while (users_.size() < count)
    users_.push_back(CreateUser(std::to_string(count)));
  users_.erase(users_.begin() + count, users_.end());

  // Notify any listeners that the user count has changed.
  data_dispatcher_.NotifyUsers(users_);
}

void LoginTestBase::TearDown() {
  if (widget_) {
    widget_->Close();
    widget_ = nullptr;
  }

  test::AshTestBase::TearDown();
}

}  // namespace ash