// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMPS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMPS_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/ssl/signed_certificate_timestamp_and_status.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class Combobox;
class Widget;
}

class SignedCertificateTimestampInfoView;
class SCTListModel;

// The Views implementation of the signed certificate timestamps viewer.
class SignedCertificateTimestampsViews : public views::DialogDelegateView,
                                         public content::NotificationObserver,
                                         public views::ComboboxListener {
 public:
  // Use ShowSignedCertificateTimestampsViewer to show.
  SignedCertificateTimestampsViews(
      content::WebContents* web_contents,
      const net::SignedCertificateTimestampAndStatusList& sct_list);
  virtual ~SignedCertificateTimestampsViews();

  // views::DialogDelegate:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::View:
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;

 private:
  void Init();

  void ShowSCTInfo(int sct_index);

  // views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  SignedCertificateTimestampInfoView* sct_info_view_;

  scoped_ptr<SCTListModel> sct_list_model_;
  // The Combobox used to select the SCT to display. This class owns the pointer
  // as it has to be deleted explicitly before the views c'tor is called:
  // The Combobox d'tor refers to the model it holds, which will be destructed
  // as part of tearing down this class. So it will not be available when the
  // Views d'tor destroys the Combobox, leading to a crash.
  // Must be deleted before the model.
  scoped_ptr<views::Combobox> sct_selector_box_;
  net::SignedCertificateTimestampAndStatusList sct_list_;

  DISALLOW_COPY_AND_ASSIGN(SignedCertificateTimestampsViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMPS_VIEWS_H_
