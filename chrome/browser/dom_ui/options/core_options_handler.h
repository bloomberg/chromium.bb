// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_CORE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_CORE_OPTIONS_HANDLER_H_
#pragma once

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/prefs/pref_change_registrar.h"

// Core options UI handler.
// Handles resource and JS calls common to all options sub-pages.
class CoreOptionsHandler : public OptionsPageUIHandler {
 public:
  CoreOptionsHandler();
  virtual ~CoreOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Uninitialize();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);

 protected:
  // Fetches a pref value of given |pref_name|.
  // Note that caller owns the returned Value.
  virtual Value* FetchPref(const std::string& pref_name);

  // Observes a pref of given |pref_name|.
  virtual void ObservePref(const std::string& pref_name);

  // Sets a pref |value| to given |pref_name|.
  virtual void SetPref(const std::string& pref_name,
                       const Value* value,
                       const std::string& metric);

  // Clears pref value for given |pref_name|.
  void ClearPref(const std::string& pref_name, const std::string& metric);

  // Stops observing given preference identified by |path|.
  virtual void StopObservingPref(const std::string& path);

  // Records a user metric action for the given value.
  void ProcessUserMetric(const Value* value,
                         const std::string& metric);

  typedef std::multimap<std::string, std::wstring> PreferenceCallbackMap;
  PreferenceCallbackMap pref_callback_map_;
 private:
  // Callback for the "coreOptionsInitialize" message.  This message will
  // trigger the Initialize() method of all other handlers so that final
  // setup can be performed before the page is shown.
  void HandleInitialize(const ListValue* args);

  // Callback for the "fetchPrefs" message. This message accepts the list of
  // preference names passed as the |args| parameter (ListValue). It passes
  // results dictionary of preference values by calling prefsFetched() JS method
  // on the page.
  void HandleFetchPrefs(const ListValue* args);

  // Callback for the "observePrefs" message. This message initiates
  // notification observing for given array of preference names.
  void HandleObservePrefs(const ListValue* args);

  // Callbacks for the "set<type>Pref" message. This message saves the new
  // preference value. |args| is an array of parameters as follows:
  //  item 0 - name of the preference.
  //  item 1 - the value of the preference in string form.
  //  item 2 - name of the metric identifier (optional).
  void HandleSetBooleanPref(const ListValue* args);
  void HandleSetIntegerPref(const ListValue* args);
  void HandleSetDoublePref(const ListValue* args);
  void HandleSetStringPref(const ListValue* args);
  void HandleSetListPref(const ListValue* args);

  void HandleSetPref(const ListValue* args, Value::ValueType type);

  // Callback for the "clearPref" message.  This message clears a preference
  // value. |args| is an array of parameters as follows:
  //  item 0 - name of the preference.
  //  item 1 - name of the metric identifier (optional).
  void HandleClearPref(const ListValue* args);

  // Callback for the "coreOptionsUserMetricsAction" message.  This records
  // an action that should be tracked if metrics recording is enabled. |args|
  // is an array that contains a single item, the name of the metric identifier.
  void HandleUserMetricsAction(const ListValue* args);

  void NotifyPrefChanged(const std::string* pref_name);

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CoreOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_CORE_OPTIONS_HANDLER_H_
