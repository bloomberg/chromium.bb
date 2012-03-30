// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
#pragma once

#include "base/callback.h"
#include "googleurl/src/gurl.h"
#include "net/base/cert_database.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

class EnrollmentDelegate;

// Dialog for certificate enrollment. This displays the content from the
// certificate enrollment URI.
class EnrollmentDialogView
    : public views::DialogDelegateView,
      public net::CertDatabase::Observer {
 public:
  virtual ~EnrollmentDialogView();

  static EnrollmentDelegate* CreateEnrollmentDelegate(
      gfx::NativeWindow owning_window);

  static void ShowDialog(gfx::NativeWindow owning_window,
                         const GURL& target_uri,
                         const base::Closure& connect);
  void Close();

  // views::DialogDelegateView overrides
  virtual int GetDialogButtons() const OVERRIDE;
  virtual void OnClose();

  // views::WidgetDelegate overrides
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::Widget overrides
  virtual views::View* GetContentsView() OVERRIDE;

  // net::CertDatabase::Observer overrides
  virtual void OnUserCertAdded(const net::X509Certificate* cert) OVERRIDE;

 private:
  EnrollmentDialogView(const GURL& target_uri,
                       const base::Closure& connect);
  void InitDialog();

  GURL target_uri_;
  base::Closure connect_;
  bool added_cert_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
