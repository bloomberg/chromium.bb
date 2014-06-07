// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/prefs/pref_member.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillDriver;
class AutofillExternalDelegate;
class AutofillClient;
struct FormData;

// Per-tab Autocomplete history manager. Handles receiving form data
// from the renderer and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public WebDataServiceConsumer {
 public:
  AutocompleteHistoryManager(AutofillDriver* driver,
                             AutofillClient* autofill_client);
  virtual ~AutocompleteHistoryManager();

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      const WDTypedResult* result) OVERRIDE;

  // Pass-through functions that are called by AutofillManager, after it has
  // dispatched a message.
  void OnGetAutocompleteSuggestions(
      int query_id,
      const base::string16& name,
      const base::string16& prefix,
      const std::string form_control_type,
      const std::vector<base::string16>& autofill_values,
      const std::vector<base::string16>& autofill_labels,
      const std::vector<base::string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids);
  virtual void OnFormSubmitted(const FormData& form);

  // Must be public for the external delegate to use.
  void OnRemoveAutocompleteEntry(const base::string16& name,
                                 const base::string16& value);

  // Sets our external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

 protected:
  friend class AutofillManagerTest;

  // Sends the given |suggestions| for display in the Autofill popup.
  void SendSuggestions(const std::vector<base::string16>* suggestions);

 private:
  // Cancels the currently pending WebDataService query, if there is one.
  void CancelPendingQuery();

  // Provides driver-level context. Must outlive this object.
  AutofillDriver* driver_;
  scoped_refptr<AutofillWebDataService> database_;

  // When the manager makes a request from WebDataServiceBase, the database is
  // queried on another thread, we record the query handle until we get called
  // back.  We also store the autofill results so we can send them together.
  WebDataServiceBase::Handle pending_query_handle_;
  int query_id_;
  std::vector<base::string16> autofill_values_;
  std::vector<base::string16> autofill_labels_;
  std::vector<base::string16> autofill_icons_;
  std::vector<int> autofill_unique_ids_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  // Delegate to provide whether or not autocomplete functionality is enabled.
  AutofillClient* const autofill_client_;

  // Whether IPC is sent.
  bool send_ipc_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteHistoryManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
