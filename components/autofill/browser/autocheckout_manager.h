// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_MANAGER_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "components/autofill/browser/autocheckout_page_meta_data.h"
#include "components/autofill/common/autocheckout_status.h"
#include "ui/gfx/native_widget_types.h"

class AutofillField;
class AutofillManager;
class AutofillProfile;
class CreditCard;
class FormStructure;
class GURL;

struct FormData;
struct FormFieldData;

namespace content {
struct SSLStatus;
}

namespace gfx {
class RectF;
}

namespace autofill {

class AutocheckoutManager {
 public:
  explicit AutocheckoutManager(AutofillManager* autofill_manager);
  virtual ~AutocheckoutManager();

  // Fill all the forms seen by the Autofill manager with the information
  // gathered from the requestAutocomplete dialog.
  void FillForms();

  // Called when clicking a proceed element in an Autocheckout flow fails.
  // |status| is the reason for the failure.
  void OnClickFailed(AutocheckoutStatus status);

  // Sets |page_meta_data_| with the meta data for the current page.
  void OnLoadedPageMetaData(
      scoped_ptr<AutocheckoutPageMetaData> page_meta_data);

  // Called when a page containing forms is loaded.
  void OnFormsSeen();

  // Causes the Autocheckout bubble to be displayed if the user hasn't seen it
  // yet for the current page. |frame_url| is the page where Autocheckout is
  // being initiated. |ssl_status| is the SSL status of the page. |native_view|
  // is the parent view of the bubble. |bounding_box| is the bounding box of the
  // input field in focus.
  virtual void MaybeShowAutocheckoutBubble(const GURL& frame_url,
                                           const content::SSLStatus& ssl_status,
                                           const gfx::NativeView& native_view,
                                           const gfx::RectF& bounding_box);

  bool is_autocheckout_bubble_showing() const {
    return is_autocheckout_bubble_showing_;
  }

 protected:
  // Exposed for testing.
  bool in_autocheckout_flow() const { return in_autocheckout_flow_; }

  // Exposed for testing.
  bool autocheckout_offered() const { return autocheckout_offered_; }

  // Show the requestAutocomplete dialog if |show_dialog| is true. Also, does
  // bookkeeping for whether or not the bubble is showing.
  virtual void MaybeShowAutocheckoutDialog(const GURL& frame_url,
                                           const content::SSLStatus& ssl_status,
                                           bool show_dialog);

 private:
  // Whether or not the current page is the start of a multipage Autofill flow.
  bool IsStartOfAutofillableFlow() const;

  // Whether or not the current page is part of a multipage Autofill flow.
  bool IsInAutofillableFlow() const;

  // Callback called from AutofillDialogController on filling up the UI form.
  void ReturnAutocheckoutData(const FormStructure* result,
                              const std::string& google_transaction_id);

  // Sends |status| to Online Wallet using AutocheckoutRequestManager.
  void SendAutocheckoutStatus(AutocheckoutStatus status);

  // Sets value of form field data |field_to_fill| based on the Autofill
  // field type specified by |field|.
  void SetValue(const AutofillField& field, FormFieldData* field_to_fill);

  AutofillManager* autofill_manager_;  // WEAK; owns us

  // Credit card verification code.
  string16 cvv_;

  // Profile built using the data supplied by requestAutocomplete dialog.
  scoped_ptr<AutofillProfile> profile_;

  // Credit card built using the data supplied by requestAutocomplete dialog.
  scoped_ptr<CreditCard> credit_card_;

  // Autocheckout specific page meta data.
  scoped_ptr<AutocheckoutPageMetaData> page_meta_data_;

  // Whether or not the Autocheckout bubble has been displayed to the user for
  // the current forms. Ensures the Autocheckout bubble is only shown to a
  // user once per pageview.
  bool autocheckout_offered_;

  // Whether or not the Autocheckout bubble is being displayed to the user.
  bool is_autocheckout_bubble_showing_;

  // Whether or not the user is in an Autocheckout flow.
  bool in_autocheckout_flow_;

  std::string google_transaction_id_;

  base::WeakPtrFactory<AutocheckoutManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_MANAGER_H_
