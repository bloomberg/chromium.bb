// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_ACTIVITY_WIDGET_DELEGATE_H_
#define ATHENA_ACTIVITY_ACTIVITY_WIDGET_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace athena {
class ActivityViewModel;

// A default WidgetDelegate for activities.
// TODO(oshima): Allow AcitivyViewModel to create custom WidgetDelegate.
class ActivityWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit ActivityWidgetDelegate(ActivityViewModel* view_model);
  virtual ~ActivityWidgetDelegate();

  // views::WidgetDelegate:
  virtual base::string16 GetWindowTitle() const override;
  virtual void DeleteDelegate() override;
  virtual views::Widget* GetWidget() override;
  virtual const views::Widget* GetWidget() const override;
  virtual views::View* GetContentsView() override;
  virtual views::ClientView* CreateClientView(views::Widget* widget) override;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

 private:
  ActivityViewModel* view_model_;

  DISALLOW_COPY_AND_ASSIGN(ActivityWidgetDelegate);
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_ACTIVITY_WIDGET_DELEGATE_H_
