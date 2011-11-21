// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_
#pragma once

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/plugin_data_remover_helper.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

// Core options UI handler.
// Handles resource and JS calls common to all options sub-pages.
class CoreOptionsHandler : public OptionsPageUIHandler {
 public:
  CoreOptionsHandler();
  virtual ~CoreOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize() OVERRIDE;
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void Uninitialize() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;

  void set_handlers_host(OptionsPageUIHandlerHost* handlers_host) {
    handlers_host_ = handlers_host;
  }

  // Adds localized strings to |localized_strings|.
  static void GetStaticLocalizedValues(
      base::DictionaryValue* localized_strings);

 protected:
  // Fetches a pref value of given |pref_name|.
  // Note that caller owns the returned Value.
  virtual base::Value* FetchPref(const std::string& pref_name);

  // Observes a pref of given |pref_name|.
  virtual void ObservePref(const std::string& pref_name);

  // Sets a pref |value| to given |pref_name|.
  virtual void SetPref(const std::string& pref_name,
                       const base::Value* value,
                       const std::string& metric);

  // Clears pref value for given |pref_name|.
  void ClearPref(const std::string& pref_name, const std::string& metric);

  // Stops observing given preference identified by |path|.
  virtual void StopObservingPref(const std::string& path);

  // Records a user metric action for the given value.
  void ProcessUserMetric(const base::Value* value,
                         const std::string& metric);

  // Notifies registered JS callbacks on change in |pref_name| preference.
  // |controlling_pref_name| controls if |pref_name| is managed by
  // policy/extension; empty |controlling_pref_name| indicates no other pref is
  // controlling |pref_name|.
  void NotifyPrefChanged(const std::string& pref_name,
                         const std::string& controlling_pref_name);

  // Creates dictionary value for |pref|, |controlling_pref| controls if |pref|
  // is managed by policy/extension; NULL indicates no other pref is controlling
  // |pref|.
  DictionaryValue* CreateValueForPref(
      const PrefService::Preference* pref,
      const PrefService::Preference* controlling_pref);

  typedef std::multimap<std::string, std::wstring> PreferenceCallbackMap;
  PreferenceCallbackMap pref_callback_map_;

 private:
  // Type of preference value received from the page. This doesn't map 1:1 to
  // Value::Type, since a TYPE_STRING can require custom processing.
  enum PrefType {
    TYPE_BOOLEAN = 0,
    TYPE_INTEGER,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_URL,
    TYPE_LIST,
  };

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
  void HandleSetURLPref(const ListValue* args);
  void HandleSetListPref(const ListValue* args);

  void HandleSetPref(const ListValue* args, PrefType type);

  // Callback for the "clearPref" message.  This message clears a preference
  // value. |args| is an array of parameters as follows:
  //  item 0 - name of the preference.
  //  item 1 - name of the metric identifier (optional).
  void HandleClearPref(const ListValue* args);

  // Callback for the "coreOptionsUserMetricsAction" message.  This records
  // an action that should be tracked if metrics recording is enabled. |args|
  // is an array that contains a single item, the name of the metric identifier.
  void HandleUserMetricsAction(const ListValue* args);

  void UpdateClearPluginLSOData();

  OptionsPageUIHandlerHost* handlers_host_;
  PrefChangeRegistrar registrar_;

  // Used for asynchronously updating the preference stating whether clearing
  // LSO data is supported.
  PluginDataRemoverHelper clear_plugin_lso_data_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CoreOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_
