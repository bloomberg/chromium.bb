// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

namespace webkit_glue {
struct FormData;
}  // namespace webkit_glue

class AutofillExternalDelegate;
class Profile;
class TabContents;

// Per-tab Autocomplete history manager. Handles receiving form data from the
// renderer and the storing and retrieving of form data through WebDataService.
class AutocompleteHistoryManager : public TabContentsObserver,
                                   public WebDataServiceConsumer {
 public:
  explicit AutocompleteHistoryManager(TabContents* tab_contents);
  virtual ~AutocompleteHistoryManager();

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
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
  void OnFormSubmitted(const webkit_glue::FormData& form);

  // Sets our external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

 protected:
  friend class AutocompleteHistoryManagerTest;
  friend class AutofillManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteHistoryManagerTest, ExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, TestTabContents);

  // For tests.
  AutocompleteHistoryManager(TabContents* tab_contents,
                             Profile* profile,
                             WebDataService* wds);

  void SendSuggestions(const std::vector<string16>* suggestions);
  void CancelPendingQuery();

  // Exposed for testing.
  AutofillExternalDelegate* external_delegate() {
    return external_delegate_;
  }

 private:
  void OnRemoveAutocompleteEntry(const string16& name, const string16& value);

  Profile* profile_;
  scoped_refptr<WebDataService> web_data_service_;

  BooleanPrefMember autofill_enabled_;

  // When the manager makes a request from WebDataService, the database is
  // queried on another thread, we record the query handle until we get called
  // back.  We also store the autofill results so we can send them together.
  WebDataService::Handle pending_query_handle_;
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

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
