// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_pref_store.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/configuration_policy_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

const ConfigurationPolicyPrefStore::PolicyToPreferenceMapEntry
    ConfigurationPolicyPrefStore::simple_policy_map_[] = {
  { Value::TYPE_STRING, kPolicyHomePage,  prefs::kHomePage },
  { Value::TYPE_BOOLEAN, kPolicyHomepageIsNewTabPage,
      prefs::kHomePageIsNewTabPage },
  { Value::TYPE_BOOLEAN, kPolicyAlternateErrorPagesEnabled,
      prefs::kAlternateErrorPagesEnabled },
  { Value::TYPE_BOOLEAN, kPolicySearchSuggestEnabled,
      prefs::kSearchSuggestEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDnsPrefetchingEnabled,
      prefs::kDnsPrefetchingEnabled },
  { Value::TYPE_BOOLEAN, kPolicySafeBrowsingEnabled,
      prefs::kSafeBrowsingEnabled },
  { Value::TYPE_BOOLEAN, kPolicyPasswordManagerEnabled,
      prefs::kPasswordManagerEnabled },
  { Value::TYPE_BOOLEAN, kPolicyMetricsReportingEnabled,
      prefs::kMetricsReportingEnabled },
  { Value::TYPE_STRING, kPolicyApplicationLocale,
      prefs::kApplicationLocale},
};

const ConfigurationPolicyPrefStore::PolicyToPreferenceMapEntry
    ConfigurationPolicyPrefStore::proxy_policy_map_[] = {
  { Value::TYPE_STRING, kPolicyProxyServer, prefs::kProxyServer },
  { Value::TYPE_STRING, kPolicyProxyPacUrl, prefs::kProxyPacUrl },
  { Value::TYPE_STRING, kPolicyProxyBypassList, prefs::kProxyBypassList }
};

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    const CommandLine* command_line,
    ConfigurationPolicyProvider* provider)
    : command_line_(command_line),
      provider_(provider),
      prefs_(new DictionaryValue()),
      command_line_proxy_settings_cleared_(false),
      proxy_disabled_(false),
      proxy_configuration_specified_(false),
      use_system_proxy_(false) {
}

void ConfigurationPolicyPrefStore::ApplyProxySwitches() {
  bool proxy_disabled = command_line_->HasSwitch(switches::kNoProxyServer);
  if (proxy_disabled) {
    prefs_->Set(prefs::kNoProxyServer, Value::CreateBooleanValue(true));
  }
  bool has_explicit_proxy_config = false;
  if (command_line_->HasSwitch(switches::kProxyAutoDetect)) {
    has_explicit_proxy_config = true;
    prefs_->Set(prefs::kProxyAutoDetect, Value::CreateBooleanValue(true));
  }
  if (command_line_->HasSwitch(switches::kProxyServer)) {
    has_explicit_proxy_config = true;
    prefs_->Set(prefs::kProxyServer, Value::CreateStringValue(
        command_line_->GetSwitchValue(switches::kProxyServer)));
  }
  if (command_line_->HasSwitch(switches::kProxyPacUrl)) {
    has_explicit_proxy_config = true;
    prefs_->Set(prefs::kProxyPacUrl, Value::CreateStringValue(
        command_line_->GetSwitchValue(switches::kProxyPacUrl)));
  }
  if (command_line_->HasSwitch(switches::kProxyBypassList)) {
    has_explicit_proxy_config = true;
    prefs_->Set(prefs::kProxyBypassList, Value::CreateStringValue(
        command_line_->GetSwitchValue(switches::kProxyBypassList)));
  }

  // Warn about all the other proxy config switches we get if
  // the --no-proxy-server command-line argument is present.
  if (proxy_disabled && has_explicit_proxy_config) {
    LOG(WARNING) << "Additional command-line proxy switches specified when --"
                 << switches::kNoProxyServer << " was also specified.";
  }
}

PrefStore::PrefReadError ConfigurationPolicyPrefStore::ReadPrefs() {
  // Initialize proxy preference values from command-line switches. This is done
  // before calling Provide to allow the provider to overwrite proxy-related
  // preferences that are specified by line settings.
  ApplyProxySwitches();

  proxy_disabled_ = false;
  proxy_configuration_specified_ = false;
  command_line_proxy_settings_cleared_ = false;

  return (provider_.get() == NULL || provider_->Provide(this)) ?
      PrefStore::PREF_READ_ERROR_NONE : PrefStore::PREF_READ_ERROR_OTHER;
}

bool ConfigurationPolicyPrefStore::ApplyProxyPolicy(PolicyType policy,
                                                    Value* value) {
  bool result = false;
  bool warn_about_proxy_disable_config = false;
  bool warn_about_proxy_system_config = false;

  const PolicyToPreferenceMapEntry* match_entry_ = NULL;
  for (const PolicyToPreferenceMapEntry* current = proxy_policy_map_;
       current != proxy_policy_map_ + arraysize(proxy_policy_map_); ++current) {
    if (current->policy_type == policy) {
      match_entry_ = current;
    }
  }

  // When the first proxy-related policy is applied, ALL proxy-related
  // preferences that have been set by command-line switches must be
  // removed. Otherwise it's possible for a user to interfere with proxy
  // policy by using proxy-related switches that are related to, but not
  // identical, to the ones set through policy.
  if ((match_entry_ ||
      policy == ConfigurationPolicyPrefStore::kPolicyProxyServerMode) &&
          !command_line_proxy_settings_cleared_) {
    for (const PolicyToPreferenceMapEntry* i = proxy_policy_map_;
        i != proxy_policy_map_ + arraysize(proxy_policy_map_); ++i) {
      if (prefs_->Get(i->preference_path, NULL)) {
        LOG(WARNING) << "proxy configuration options were specified on the"
                     << " command-line but will be ignored because an"
                     << " explicit proxy configuration has been specified"
                     << " through a centrally-administered policy.";
        break;
      }
    }

    // Now actually do the preference removal.
    for (const PolicyToPreferenceMapEntry* current = proxy_policy_map_;
        current != proxy_policy_map_ + arraysize(proxy_policy_map_); ++current)
      prefs_->Remove(current->preference_path, NULL);
    prefs_->Remove(prefs::kNoProxyServer, NULL);
    prefs_->Remove(prefs::kProxyAutoDetect, NULL);

    command_line_proxy_settings_cleared_ = true;
  }

  // Translate the proxy policy into preferences.
  if (policy == ConfigurationPolicyStore::kPolicyProxyServerMode) {
    int int_value;
    bool proxy_auto_detect = false;
    if (value->GetAsInteger(&int_value)) {
      result = true;
      switch (int_value) {
        case ConfigurationPolicyStore::kPolicyNoProxyServerMode:
          if (!proxy_disabled_) {
            if (proxy_configuration_specified_)
              warn_about_proxy_disable_config = true;
            proxy_disabled_ = true;
          }
          break;
        case ConfigurationPolicyStore::kPolicyAutoDetectProxyMode:
          proxy_auto_detect = true;
          break;
        case ConfigurationPolicyStore::kPolicyManuallyConfiguredProxyMode:
          break;
        case ConfigurationPolicyStore::kPolicyUseSystemProxyMode:
          if (!use_system_proxy_) {
            if (proxy_configuration_specified_)
              warn_about_proxy_system_config = true;
            use_system_proxy_ = true;
          }
          break;
        default:
          // Not a valid policy, don't assume ownership of |value|
          result = false;
          break;
      }

      if (int_value != kPolicyUseSystemProxyMode) {
        prefs_->Set(prefs::kNoProxyServer,
                    Value::CreateBooleanValue(proxy_disabled_));
        prefs_->Set(prefs::kProxyAutoDetect,
                    Value::CreateBooleanValue(proxy_auto_detect));
      }

      // No proxy and system proxy mode should ensure that no other
      // proxy preferences are set.
      if (int_value == ConfigurationPolicyStore::kPolicyNoProxyServerMode ||
          int_value == kPolicyUseSystemProxyMode) {
        for (const PolicyToPreferenceMapEntry* current = proxy_policy_map_;
            current != proxy_policy_map_ + arraysize(proxy_policy_map_);
            ++current)
          prefs_->Remove(current->preference_path, NULL);
      }
    }
  } else if (match_entry_) {
    // Determine if the applied proxy policy settings conflict and issue
    // a corresponding warning if they do.
    if (!proxy_configuration_specified_) {
      if (proxy_disabled_)
        warn_about_proxy_disable_config = true;
      if (use_system_proxy_)
        warn_about_proxy_system_config = true;
      proxy_configuration_specified_ = true;
    }
    if (!use_system_proxy_ && !proxy_disabled_) {
      prefs_->Set(match_entry_->preference_path, value);
      // The ownership of value has been passed on to |prefs_|,
      // don't clean it up later.
      value = NULL;
    }
    result = true;
  }

  if (warn_about_proxy_disable_config) {
    LOG(WARNING) << "A centrally-administered policy disables the use of"
                 << " a proxy but also specifies an explicit proxy"
                 << " configuration.";
  }

  if (warn_about_proxy_system_config) {
    LOG(WARNING) << "A centrally-administered policy dictates that the"
                 << " system proxy settings should be used but also specifies"
                 << " an explicit proxy configuration.";
  }

  // If the policy was a proxy policy, cleanup |value|.
  if (result && value)
    delete value;
  return result;
}

bool ConfigurationPolicyPrefStore::ApplyPluginPolicy(PolicyType policy,
                                                     Value* value) {
  if (policy == kPolicyDisabledPlugins) {
    string16 plugin_list;
    if (value->GetAsUTF16(&plugin_list)) {
      std::vector<string16> plugin_names;
      // Change commas into tabs so that we can change escaped
      // tabs back into commas, leaving non-escaped commas as tabs
      // that can be used for splitting the string. Note that plugin
      // names must not contain backslashes, since a trailing backslash
      // in a plugin name before a comma would get swallowed during the
      // splitting.
      std::replace(plugin_list.begin(), plugin_list.end(), L',', L'\t');
      ReplaceSubstringsAfterOffset(&plugin_list, 0,
                                   ASCIIToUTF16("\\\t"), ASCIIToUTF16(","));
      SplitString(plugin_list, L'\t', &plugin_names);
      bool added_plugin = false;
      scoped_ptr<ListValue> list(new ListValue());
      for (std::vector<string16>::const_iterator i(plugin_names.begin());
           i != plugin_names.end(); ++i) {
        if (!i->empty()) {
          list->Append(Value::CreateStringValueFromUTF16(*i));
          added_plugin = true;
        }
      }
      if (added_plugin) {
        prefs_->Set(prefs::kPluginsPluginsBlacklist, list.release());
        delete value;
        return true;
      }
    }
  }
  return false;
}

bool ConfigurationPolicyPrefStore::ApplySyncPolicy(PolicyType policy,
                                                   Value* value) {
  if (policy == ConfigurationPolicyStore::kPolicySyncDisabled) {
    bool disable_sync;
    if (value->GetAsBoolean(&disable_sync) && disable_sync)
      prefs_->Set(prefs::kSyncManaged, value);
    else
      delete value;
    return true;
  }
  return false;
}

bool ConfigurationPolicyPrefStore::ApplyPolicyFromMap(PolicyType policy,
    Value* value, const PolicyToPreferenceMapEntry map[], int size) {
  const PolicyToPreferenceMapEntry* end = map + size;
  for (const PolicyToPreferenceMapEntry* current = map;
       current != end; ++current) {
    if (current->policy_type == policy) {
      DCHECK(current->value_type == value->GetType());
      prefs_->Set(current->preference_path, value);
      return true;
    }
  }
  return false;
}

void ConfigurationPolicyPrefStore::Apply(PolicyType policy, Value* value) {
  if (ApplyProxyPolicy(policy, value))
    return;

  if (ApplyPluginPolicy(policy, value))
    return;

  if (ApplySyncPolicy(policy, value))
    return;

  if (ApplyPolicyFromMap(policy, value, simple_policy_map_,
                         arraysize(simple_policy_map_)))
    return;

  // Other policy implementations go here.
  NOTIMPLEMENTED();
  delete value;
}
