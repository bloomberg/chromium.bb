// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_MANAGER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/autocheckout_page_meta_data.h"
#include "components/autofill/content/browser/autocheckout_statistic.h"
#include "components/autofill/core/browser/autocheckout_bubble_state.h"
#include "components/autofill/core/common/autocheckout_status.h"

class GURL;

namespace gfx {
class RectF;
}

namespace net {
class URLRequestContextGetter;
}

namespace autofill {

class AutofillField;
class AutofillManager;
class AutofillMetrics;
class AutofillProfile;
class CreditCard;
class FormStructure;

struct FormData;
struct FormFieldData;

class AutocheckoutManager {
 public:
  explicit AutocheckoutManager(AutofillManager* autofill_manager);
  virtual ~AutocheckoutManager();

  // Fill all the forms seen by the Autofill manager with the information
  // gathered from the requestAutocomplete dialog.
  void FillForms();

  // Called to signal that the renderer has completed processing a page in the
  // Autocheckout flow. |status| is the reason for the failure, or |SUCCESS| if
  // there were no errors.
  void OnAutocheckoutPageCompleted(AutocheckoutStatus status);

  // Sets |page_meta_data_| with the meta data for the current page.
  void OnLoadedPageMetaData(
      scoped_ptr<AutocheckoutPageMetaData> page_meta_data);

  // Called when a page containing forms is loaded.
  void OnFormsSeen();

  // Whether ajax on the current page should be ignored during
  // an Autocheckout flow.
  bool ShouldIgnoreAjax();

  // Causes the Autocheckout bubble to be displayed if the user hasn't seen it
  // yet for the current page. |frame_url| is the page where Autocheckout is
  // being initiated. |bounding_box| is the bounding box of the input field in
  // focus.
  virtual void MaybeShowAutocheckoutBubble(const GURL& frame_url,
                                           const gfx::RectF& bounding_box);

  // Determine whether we should keep the dialog visible.
  bool should_preserve_dialog() const { return should_preserve_dialog_; }

  void set_should_show_bubble(bool should_show_bubble) {
    should_show_bubble_ = should_show_bubble;
  }

  bool is_autocheckout_bubble_showing() const {
    return is_autocheckout_bubble_showing_;
  }

 protected:
  // Exposed for testing.
  bool in_autocheckout_flow() const { return in_autocheckout_flow_; }

  // Exposed for testing.
  bool should_show_bubble() const { return should_show_bubble_; }

  // Show the requestAutocomplete dialog if |state| is
  // AUTOCHECKOUT_BUBBLE_ACCEPTED. Also, does bookkeeping for whether or not
  // the bubble is showing.
  virtual void MaybeShowAutocheckoutDialog(const GURL& frame_url,
                                           AutocheckoutBubbleState state);

  // Callback called from AutofillDialogController on filling up the UI form.
  void ReturnAutocheckoutData(const FormStructure* result,
                              const std::string& google_transaction_id);

  const AutofillMetrics& metric_logger() const { return *metric_logger_; }
  void set_metric_logger(scoped_ptr<AutofillMetrics> metric_logger);

 private:
  // Shows the Autocheckout bubble. Must be called on the UI thread. |frame_url|
  // is the page where Autocheckout is being initiated. |bounding_box| is the
  // bounding box of the input field in focus. |cookies| is any Google Account
  // cookies.
  void ShowAutocheckoutBubble(const GURL& frame_url,
                              const gfx::RectF& bounding_box,
                              const std::string& cookies);

  // Whether or not the current page is the start of a multipage Autofill flow.
  bool IsStartOfAutofillableFlow() const;

  // Whether or not the current page is part of a multipage Autofill flow.
  bool IsInAutofillableFlow() const;

  // Sends |status| to Online Wallet using AutocheckoutRequestManager.
  void SendAutocheckoutStatus(AutocheckoutStatus status);

  // Sets value of form field data |field_to_fill| based on the Autofill
  // field type specified by |field|.
  void SetValue(const AutofillField& field, FormFieldData* field_to_fill);

  // Sets the progress of all steps for the given page to the provided value.
  void SetStepProgressForPage(int page_number, AutocheckoutStepStatus status);

  // Account time spent between now and |last_step_completion_timestamp_|
  // towards |page_number|.
  void RecordTimeTaken(int page_number);

  // Terminate the Autocheckout flow and send Autocheckout status to Wallet
  // server.
  void EndAutocheckout(AutocheckoutStatus status);

  AutofillManager* autofill_manager_;  // WEAK; owns us

  // Credit card verification code.
  base::string16 cvv_;

  // Profile built using the data supplied by requestAutocomplete dialog.
  scoped_ptr<AutofillProfile> profile_;

  // Credit card built using the data supplied by requestAutocomplete dialog.
  scoped_ptr<CreditCard> credit_card_;

  // Billing address built using data supplied by requestAutocomplete dialog.
  scoped_ptr<AutofillProfile> billing_address_;

  // Autocheckout specific page meta data of current page.
  scoped_ptr<AutocheckoutPageMetaData> page_meta_data_;

  scoped_ptr<AutofillMetrics> metric_logger_;

  // Whether or not the Autocheckout bubble should be shown to user.
  bool should_show_bubble_;

  // Whether or not the Autocheckout bubble is being displayed to the user.
  bool is_autocheckout_bubble_showing_;

  // Whether or not the user is in an Autocheckout flow.
  bool in_autocheckout_flow_;

  // Whether or not the currently visible dialog, if there is one, should be
  // preserved.
  bool should_preserve_dialog_;

  // AutocheckoutStepTypes for the various pages of the flow.
  std::map<int, std::vector<AutocheckoutStepType> > page_types_;

  // Timestamp of last step's completion.
  base::TimeTicks last_step_completion_timestamp_;

  // Per page latency statistics.
  std::vector<AutocheckoutStatistic> latency_statistics_;

  std::string google_transaction_id_;

  base::WeakPtrFactory<AutocheckoutManager> weak_ptr_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_MANAGER_H_
