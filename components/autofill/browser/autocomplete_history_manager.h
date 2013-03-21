// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/api/webdata/autofill_web_data_service.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "content/public/browser/web_contents_observer.h"

struct FormData;

namespace content {
class BrowserContext;
}

class AutofillExternalDelegate;

// Per-tab Autocomplete history manager. Handles receiving form data
// from the renderer and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public content::WebContentsObserver,
                                   public WebDataServiceConsumer {
 public:
  explicit AutocompleteHistoryManager(content::WebContents* web_contents);
  virtual ~AutocompleteHistoryManager();

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      const WDTypedResult* result) OVERRIDE;

  // Pass-through functions that are called by AutofillManager, after it has
  // dispatched a message.
  void OnGetAutocompleteSuggestions(
      int query_id,
      const string16& name,
      const string16& prefix,
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids);
  void OnFormSubmitted(const FormData& form);

  // Must be public for the external delegate to use.
  void OnRemoveAutocompleteEntry(const string16& name, const string16& value);

  // Sets our external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

 protected:
  friend class AutofillManagerTest;

  // Sends the given |suggestions| for display in the Autofill popup.
  void SendSuggestions(const std::vector<string16>* suggestions);

 private:
  // Cancels the currently pending WebDataService query, if there is one.
  void CancelPendingQuery();

  content::BrowserContext* browser_context_;
  scoped_refptr<AutofillWebDataService> autofill_data_;

  BooleanPrefMember autofill_enabled_;

  // When the manager makes a request from WebDataServiceBase, the database is
  // queried on another thread, we record the query handle until we get called
  // back.  We also store the autofill results so we can send them together.
  WebDataServiceBase::Handle pending_query_handle_;
  int query_id_;
  std::vector<string16> autofill_values_;
  std::vector<string16> autofill_labels_;
  std::vector<string16> autofill_icons_;
  std::vector<int> autofill_unique_ids_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteHistoryManager);
};

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
