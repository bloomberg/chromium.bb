// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class ProtocolDialogDelegate;

namespace views {
class MessageBoxView;
}

class ExternalProtocolDialog : public views::DialogDelegate {
 public:
  // RunExternalProtocolDialog calls this private constructor.
  ExternalProtocolDialog(scoped_ptr<const ProtocolDialogDelegate> delegate,
                         int render_process_host_id,
                         int routing_id);

  virtual ~ExternalProtocolDialog();

  // views::DialogDelegate methods:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:
  const scoped_ptr<const ProtocolDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  // IDs of the associated WebContents.
  int render_process_host_id_;
  int routing_id_;

  // The time at which this dialog was created.
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
