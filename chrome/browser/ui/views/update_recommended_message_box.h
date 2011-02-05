// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_
#define CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;

// A dialog box that tells the user that an update is recommended in order for
// the latest version to be put to use.
class UpdateRecommendedMessageBox : public views::DialogDelegate {
 public:
  // This box is modal to |parent_window|.
  static void ShowMessageBox(gfx::NativeWindow parent_window);

  // Overridden from views::DialogDelegate:
  virtual bool Accept();

 protected:
  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;

  // Overridden from views::WindowDelegate:
  virtual void DeleteDelegate();
  virtual bool IsModal() const;
  virtual views::View* GetContentsView();

 private:
  explicit UpdateRecommendedMessageBox(gfx::NativeWindow parent_window);
  virtual ~UpdateRecommendedMessageBox();

  MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecommendedMessageBox);
};

#endif  // CHROME_BROWSER_UI_VIEWS_UPDATE_RECOMMENDED_MESSAGE_BOX_H_
