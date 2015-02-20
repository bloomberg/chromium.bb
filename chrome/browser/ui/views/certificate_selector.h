// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "net/cert/x509_certificate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class LabelButton;
class TableView;
}

namespace chrome {

// A base class for dialogs that show a given list of certificates to the user.
// The user can select a single certificate and look at details of each
// certificate.
// The currently selected certificate can be obtained using |GetSelectedCert()|.
// The explanatory text shown to the user must be provided to |InitWithText()|.
class CertificateSelector : public views::DialogDelegateView,
                            public views::ButtonListener,
                            public views::TableViewObserver {
 public:
  class CertificateTableModel;

  // |web_contents| must not be null.
  CertificateSelector(const net::CertificateList& certificates,
                      content::WebContents* web_contents);
  ~CertificateSelector() override;

  // Returns the currently selected certificate or null if none is selected.
  // Must be called after |InitWithText()|.
  net::X509Certificate* GetSelectedCert() const;

  // Shows this dialog as a constrained web modal dialog and focuses the first
  // certificate.
  // Must be called after |InitWithText()|.
  void Show();

  // DialogDelegateView:
  bool CanResize() const override;
  base::string16 GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  views::View* CreateExtraView() override;
  ui::ModalType GetModalType() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;

 protected:
  // Initializes the dialog. |text| is shown above the list of certificates
  // and is supposed to explain to the user what the implication of the
  // certificate selection is.
  void InitWithText(const base::string16& text);

 private:
  const net::CertificateList certificates_;
  scoped_ptr<CertificateTableModel> model_;

  content::WebContents* const web_contents_;

  views::TableView* table_;
  views::LabelButton* view_cert_button_;

  DISALLOW_COPY_AND_ASSIGN(CertificateSelector);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_
