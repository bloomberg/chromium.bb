// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class ProtocolDialogDelegate;

namespace test {
class ExternalProtocolDialogTestApi;
}

namespace views {
class MessageBoxView;
}

class ExternalProtocolDialog : public views::DialogDelegate {
 public:
  // RunExternalProtocolDialog calls this private constructor.
  ExternalProtocolDialog(std::unique_ptr<const ProtocolDialogDelegate> delegate,
                         int render_process_host_id,
                         int routing_id);

  ~ExternalProtocolDialog() override;

  // views::DialogDelegate methods:
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  ui::ModalType GetModalType() const override;

 private:
  friend class test::ExternalProtocolDialogTestApi;

  const std::unique_ptr<const ProtocolDialogDelegate> delegate_;

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
