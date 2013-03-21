// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/plugins/plugin_status_pref_setter.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Core options UI handler.
// Handles resource and JS calls common to all options sub-pages.
class CoreOptionsHandler : public OptionsPageUIHandler {
 public:
  CoreOptionsHandler();
  virtual ~CoreOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void Uninitialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

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

  // Stops observing given preference identified by |pref_name|.
  virtual void StopObservingPref(const std::string& pref_name);

  // Sets a pref |value| to given |pref_name|.
  virtual void SetPref(const std::string& pref_name,
                       const base::Value* value,
                       const std::string& metric);

  // Clears pref value for given |pref_name|.
  void ClearPref(const std::string& pref_name, const std::string& metric);

  // Records a user metric action for the given value.
  void ProcessUserMetric(const base::Value* value,
                         const std::string& metric);

  // Virtual dispatch is needed as handling of some prefs may be
  // finessed in subclasses.  The PrefService pointer is included
  // so that subclasses can know whether the observed pref is from the
  // local state or not.
  virtual void OnPreferenceChanged(PrefService* service,
                                   const std::string& pref_name);

  // Notifies registered JS callbacks on change in |pref_name| preference.
  // |controlling_pref_name| controls if |pref_name| is managed by
  // policy/extension; empty |controlling_pref_name| indicates no other pref is
  // controlling |pref_name|.
  void NotifyPrefChanged(const std::string& pref_name,
                         const std::string& controlling_pref_name);

  // Calls JS callbacks to report a change in the value of the |name|
  // preference. |value| is the new value for |name|.  Called from
  // Notify*Changed methods to fire off the notifications.
  void DispatchPrefChangeNotification(const std::string& name,
                                      scoped_ptr<base::Value> value);

  // Creates dictionary value for the pref described by |pref_name|.
  // If |controlling_pref| is not empty, it describes the pref that manages
  // |pref| via policy or extension.
  base::Value* CreateValueForPref(const std::string& pref_name,
                                  const std::string& controlling_pref_name);

  typedef std::multimap<std::string, std::string> PreferenceCallbackMap;
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

  // Finds the pref service that holds the given pref. If the pref is not found,
  // it will return user prefs.
  PrefService* FindServiceForPref(const std::string& pref_name);

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
  void UpdatePepperFlashSettingsEnabled();

  OptionsPageUIHandlerHost* handlers_host_;
  // This registrar keeps track of user prefs.
  PrefChangeRegistrar registrar_;
  // This registrar keeps track of local state.
  PrefChangeRegistrar local_state_registrar_;

  PluginStatusPrefSetter plugin_status_pref_setter_;

  // This maps pref names to filter functions. The callbacks should take the
  // value that the user has attempted to set for the pref, and should return
  // true if that value may be applied. If the return value is false, the
  // change will be ignored.
  typedef std::map<std::string, base::Callback<bool(const base::Value*)> >
      PrefChangeFilterMap;
  PrefChangeFilterMap pref_change_filters_;

  DISALLOW_COPY_AND_ASSIGN(CoreOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CORE_OPTIONS_HANDLER_H_
