// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FORM_FIELD_HISTORY_MANAGER_H_
#define CHROME_BROWSER_FORM_FIELD_HISTORY_MANAGER_H_

#include <string>

#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/pref_member.h"

namespace webkit_glue {
class FormFieldValues;
}

class Profile;
class TabContents;

// Per-tab autofill manager. Handles receiving form data from the renderer and
// the storing and retrieving of form data through WebDataService.
class FormFieldHistoryManager : public RenderViewHostDelegate::FormFieldHistory,
                                public WebDataServiceConsumer {
 public:
  explicit FormFieldHistoryManager(TabContents* tab_contents);
  virtual ~FormFieldHistoryManager();

  Profile* profile();

  // RenderViewHostDelegate::FormFieldHistory implementation.
  virtual void FormFieldValuesSubmitted(
      const webkit_glue::FormFieldValues& form);
  virtual bool GetFormFieldHistorySuggestions(int query_id,
                                              const string16& name,
                                              const string16& prefix);
  virtual void RemoveFormFieldHistoryEntry(const string16& name,
                                           const string16& value);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  static void RegisterUserPrefs(PrefService* prefs);

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

  DISALLOW_COPY_AND_ASSIGN(FormFieldHistoryManager);
};

#endif  // CHROME_BROWSER_FORM_FIELD_HISTORY_MANAGER_H_
