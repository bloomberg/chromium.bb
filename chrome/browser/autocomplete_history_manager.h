// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <string>

#include "chrome/browser/pref_member.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/webdata/web_data_service.h"

namespace webkit_glue {
class FormFieldValues;
}

class Profile;
class TabContents;

// Per-tab Autocomplete history manager. Handles receiving form data from the
// renderer and the storing and retrieving of form data through WebDataService.
class AutocompleteHistoryManager
    : public RenderViewHostDelegate::Autocomplete,
      public WebDataServiceConsumer {
 public:
  explicit AutocompleteHistoryManager(TabContents* tab_contents);
  virtual ~AutocompleteHistoryManager();

  Profile* profile();

  // RenderViewHostDelegate::Autocomplete implementation.
  virtual void FormFieldValuesSubmitted(
      const webkit_glue::FormFieldValues& form);
  virtual bool GetAutocompleteSuggestions(int query_id,
                                          const string16& name,
                                          const string16& prefix);
  virtual void RemoveAutocompleteEntry(const string16& name,
                                       const string16& value);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

 private:
  void CancelPendingQuery();
  void StoreFormEntriesInWebDatabase(const webkit_glue::FormFieldValues& form);
  void SendSuggestions(const WDTypedResult* suggestions);

  TabContents* tab_contents_;

  BooleanPrefMember form_autofill_enabled_;

  // When the manager makes a request from WebDataService, the database
  // is queried on another thread, we record the query handle until we
  // get called back.
  WebDataService::Handle pending_query_handle_;
  int query_id_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteHistoryManager);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
