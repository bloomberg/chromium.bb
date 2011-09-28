// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "views/window/dialog_delegate.h"

class TabContents;

namespace views {
class MessageBoxView;
}

class ExternalProtocolDialog : public views::DialogDelegate {
 public:
  // RunExternalProtocolDialog calls this private constructor.
  ExternalProtocolDialog(TabContents* tab_contents,
                         const GURL& url,
                         const std::wstring& command);

  // Returns the path of the application to be launched given the protocol
  // of the requested url. Returns an empty string on failure.
  static std::wstring GetApplicationForProtocol(const GURL& url);

  virtual ~ExternalProtocolDialog();

  // views::DialogDelegate methods:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // views::WidgetDelegate methods:
  virtual bool IsModal() const OVERRIDE { return false; }

 private:
  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  // The associated TabContents.
  TabContents* tab_contents_;

  // URL of the external protocol request.
  GURL url_;

  // The time at which this dialog was created.
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
