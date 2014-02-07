// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMP_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMP_INFO_VIEW_H_

#include <vector>

#include "net/cert/sct_status_flags.h"
#include "ui/views/view.h"

namespace views {
class GridLayout;
class Label;
class Textfield;
}

namespace net {
namespace ct {
struct SignedCertificateTimestamp;
}
}

namespace chrome {
namespace ct {

// Utility function to translate an SCT status to resource ID.
int StatusToResourceID(net::ct::SCTVerifyStatus status);
int SCTOriginToResourceID(const net::ct::SignedCertificateTimestamp& sct);

}  // namespace ct
}  // namespace chrome

// Responsible for displaying a tabular grid of SCT information.
// Only displays information about one SCT at a time.
class SignedCertificateTimestampInfoView : public views::View {
 public:
  SignedCertificateTimestampInfoView();
  virtual ~SignedCertificateTimestampInfoView();

  // Updates the display to show information for the given SCT.
  void SetSignedCertificateTimestamp(
      const net::ct::SignedCertificateTimestamp& sct,
      net::ct::SCTVerifyStatus status);

  // Clears the SCT display to indicate that no SCT is selected.
  void ClearDisplay();

 private:
  // views::View implementation
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;

  // Layout helper routines.
  void AddLabelRow(int layout_id,
                   views::GridLayout* layout,
                   int label_message_id,
                   views::Textfield* text_field);

  // Sets up the view layout.
  void Init();

  // Individual property labels
  views::Textfield* status_value_field_;
  views::Textfield* origin_value_field_;
  views::Textfield* version_value_field_;
  views::Textfield* log_description_value_field_;
  views::Textfield* log_id_value_field_;
  views::Textfield* timestamp_value_field_;
  views::Textfield* hash_algorithm_value_field_;
  views::Textfield* signature_algorithm_value_field_;
  views::Textfield* signature_data_value_field_;

  DISALLOW_COPY_AND_ASSIGN(SignedCertificateTimestampInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIGNED_CERTIFICATE_TIMESTAMP_INFO_VIEW_H_
