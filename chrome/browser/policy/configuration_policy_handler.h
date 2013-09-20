// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/common/content_settings.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Maps a policy type to a preference path, and to the expected value type.
struct PolicyToPreferenceMapEntry {
  const char* const policy_name;
  const char* const preference_path;
  const base::Value::Type value_type;
};

// An abstract super class that subclasses should implement to map policies to
// their corresponding preferences, and to check whether the policies are valid.
class ConfigurationPolicyHandler {
 public:
  static std::string ValueTypeToString(Value::Type type);

  ConfigurationPolicyHandler();
  virtual ~ConfigurationPolicyHandler();

  // Returns whether the policy settings handled by this
  // ConfigurationPolicyHandler can be applied.  Fills |errors| with error
  // messages or warnings.  |errors| may contain error messages even when
  // |CheckPolicySettings()| returns true.
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) = 0;

  // Processes the policies handled by this ConfigurationPolicyHandler and sets
  // the appropriate preferences in |prefs|.
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) = 0;

  // Modifies the values of some of the policies in |policies| so that they
  // are more suitable to display to the user. This can be used to remove
  // sensitive values such as passwords, or to pretty-print values.
  // The base implementation just converts DictionaryValue policies to a
  // StringValue representation.
  virtual void PrepareForDisplaying(PolicyMap* policies) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyHandler);
};

// Abstract class derived from ConfigurationPolicyHandler that should be
// subclassed to handle a single policy (not a combination of policies).
class TypeCheckingPolicyHandler : public ConfigurationPolicyHandler {
 public:
  TypeCheckingPolicyHandler(const char* policy_name,
                            base::Value::Type value_type);
  virtual ~TypeCheckingPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;

  const char* policy_name() const;

 protected:
  // Runs policy checks and returns the policy value if successful.
  bool CheckAndGetValue(const PolicyMap& policies,
                        PolicyErrorMap* errors,
                        const Value** value);

 private:
  // The name of the policy.
  const char* policy_name_;

  // The type the value of the policy should have.
  base::Value::Type value_type_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckingPolicyHandler);
};

// Abstract class derived from TypeCheckingPolicyHandler that ensures an int
// policy's value lies in an allowed range. Either clamps or rejects values
// outside the range.
class IntRangePolicyHandlerBase : public TypeCheckingPolicyHandler {
 public:
  IntRangePolicyHandlerBase(const char* policy_name,
                            int min,
                            int max,
                            bool clamp);

  // ConfigurationPolicyHandler:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;

 protected:
  virtual ~IntRangePolicyHandlerBase();

  // Ensures that the value is in the allowed range. Returns false if the value
  // cannot be parsed or lies outside the allowed range and clamping is
  // disabled.
  bool EnsureInRange(const base::Value* input,
                     int* output,
                     PolicyErrorMap* errors);

 private:
  // The minimum value allowed.
  int min_;

  // The maximum value allowed.
  int max_;

  // Whether to clamp values lying outside the allowed range instead of
  // rejecting them.
  bool clamp_;

  DISALLOW_COPY_AND_ASSIGN(IntRangePolicyHandlerBase);
};

// ConfigurationPolicyHandler for policies that map directly to a preference.
class SimplePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SimplePolicyHandler(const char* policy_name,
                      const char* pref_path,
                      base::Value::Type value_type);
  virtual ~SimplePolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // The DictionaryValue path of the preference the policy maps to.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(SimplePolicyHandler);
};

// A policy handler implementation that maps a string enum list to an int enum
// list as specified by a mapping table.
class StringToIntEnumListPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  struct MappingEntry {
    const char* enum_value;
    int int_value;
  };

  StringToIntEnumListPolicyHandler(const char* policy_name,
                                   const char* pref_path,
                                   const MappingEntry* mapping_begin,
                                   const MappingEntry* mapping_end);

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Attempts to convert the list in |input| to |output| according to the table,
  // returns false on errors.
  bool Convert(const base::Value* input,
               base::ListValue* output,
               PolicyErrorMap* errors);

  // Name of the pref to write.
  const char* pref_path_;

  // The mapping table.
  const MappingEntry* mapping_begin_;
  const MappingEntry* mapping_end_;

  DISALLOW_COPY_AND_ASSIGN(StringToIntEnumListPolicyHandler);
};

// A policy handler implementation that ensures an int policy's value lies in an
// allowed range.
class IntRangePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  IntRangePolicyHandler(const char* policy_name,
                        const char* pref_path,
                        int min,
                        int max,
                        bool clamp);
  virtual ~IntRangePolicyHandler();

  // ConfigurationPolicyHandler:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Name of the pref to write.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(IntRangePolicyHandler);
};

// A policy handler implementation that maps an int percentage value to a
// double.
class IntPercentageToDoublePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  IntPercentageToDoublePolicyHandler(const char* policy_name,
                                     const char* pref_path,
                                     int min,
                                     int max,
                                     bool clamp);
  virtual ~IntPercentageToDoublePolicyHandler();

  // ConfigurationPolicyHandler:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Name of the pref to write.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(IntPercentageToDoublePolicyHandler);
};

// Implements additional checks for policies that are lists of extension IDs.
class ExtensionListPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ExtensionListPolicyHandler(const char* policy_name,
                             const char* pref_path,
                             bool allow_wildcards);
  virtual ~ExtensionListPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 protected:
  const char* pref_path() const;

  // Runs sanity checks on the policy value and returns it in |extension_ids|.
  bool CheckAndGetList(const PolicyMap& policies,
                       PolicyErrorMap* errors,
                       scoped_ptr<base::ListValue>* extension_ids);

 private:
  const char* pref_path_;
  bool allow_wildcards_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionListPolicyHandler);
};

class ExtensionInstallForcelistPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  explicit ExtensionInstallForcelistPolicyHandler(const char* pref_name);
  virtual ~ExtensionInstallForcelistPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Parses the data in |policy_value| and writes them to |extension_dict|.
  bool ParseList(const base::Value* policy_value,
                 base::DictionaryValue* extension_dict,
                 PolicyErrorMap* errors);

  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallForcelistPolicyHandler);
};

// Implements additional checks for policies that are lists of extension
// URLPatterns.
class ExtensionURLPatternListPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ExtensionURLPatternListPolicyHandler(const char* policy_name,
                                       const char* pref_path);
  virtual ~ExtensionURLPatternListPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionURLPatternListPolicyHandler);
};

// ConfigurationPolicyHandler for the SyncDisabled policy.
class SyncPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit SyncPolicyHandler(const char* pref_name);
  virtual ~SyncPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(SyncPolicyHandler);
};

// ConfigurationPolicyHandler for the AutofillEnabled policy.
class AutofillPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit AutofillPolicyHandler(const char* pref_name);
  virtual ~AutofillPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(AutofillPolicyHandler);
};

#if !defined(OS_ANDROID)

// ConfigurationPolicyHandler for the DownloadDirectory policy.
class DownloadDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DownloadDirPolicyHandler(const char* default_directory_pref_name,
                           const char* prompt_for_download_pref_name);
  virtual ~DownloadDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* default_directory_pref_name_;
  const char* prompt_for_download_pref_name_;
  DISALLOW_COPY_AND_ASSIGN(DownloadDirPolicyHandler);
};

// ConfigurationPolicyHandler for the DiskCacheDir policy.
class DiskCacheDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit DiskCacheDirPolicyHandler(const char* pref_name);
  virtual ~DiskCacheDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(DiskCacheDirPolicyHandler);
};

#endif  // !defined(OS_ANDROID)

// ConfigurationPolicyHandler for the FileSelectionDialogsHandler policy.
class FileSelectionDialogsHandler : public TypeCheckingPolicyHandler {
 public:
  FileSelectionDialogsHandler(const char* allow_dialogs_pref_name,
                              const char* prompt_for_download_pref_name);
  virtual ~FileSelectionDialogsHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* allow_dialogs_pref_name_;
  const char* prompt_for_download_pref_name_;
  DISALLOW_COPY_AND_ASSIGN(FileSelectionDialogsHandler);
};

// ConfigurationPolicyHandler for the incognito mode policies.
class IncognitoModePolicyHandler : public ConfigurationPolicyHandler {
 public:
  explicit IncognitoModePolicyHandler(const char* pref_name);
  virtual ~IncognitoModePolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  IncognitoModePrefs::Availability GetAvailabilityValueAsEnum(
      const Value* availability);

  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(IncognitoModePolicyHandler);
};

// ConfigurationPolicyHandler for the DefaultSearchEncodings policy.
class DefaultSearchEncodingsPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit DefaultSearchEncodingsPolicyHandler(const char* pref_name);
  virtual ~DefaultSearchEncodingsPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

  const char* pref_name() const { return pref_name_; }

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(DefaultSearchEncodingsPolicyHandler);
};

enum DefaultSearchKey {
  DEFAULT_SEARCH_ENABLED,
  DEFAULT_SEARCH_NAME,
  DEFAULT_SEARCH_KEYWORD,
  DEFAULT_SEARCH_SEARCH_URL,
  DEFAULT_SEARCH_SUGGEST_URL,
  DEFAULT_SEARCH_INSTANT_URL,
  DEFAULT_SEARCH_ICON_URL,
  DEFAULT_SEARCH_ENCODINGS,
  DEFAULT_SEARCH_ALTERNATE_URLS,
  DEFAULT_SEARCH_TERMS_REPLACEMENT_KEY,
  DEFAULT_SEARCH_IMAGE_URL,
  DEFAULT_SEARCH_NEW_TAB_URL,
  DEFAULT_SEARCH_SEARCH_URL_POST_PARAMS,
  DEFAULT_SEARCH_SUGGEST_URL_POST_PARAMS,
  DEFAULT_SEARCH_INSTANT_URL_POST_PARAMS,
  DEFAULT_SEARCH_IMAGE_URL_POST_PARAMS,
  // Must be last.
  DEFAULT_SEARCH_KEY_SIZE,
};

// ConfigurationPolicyHandler for the default search policies.
class DefaultSearchPolicyHandler : public ConfigurationPolicyHandler {
 public:
   // Constructs a new handler for the DefaultSearch policy with the specified
   // preference names.
   // |id_pref_name|: Pref name for the search provider ID.
   // |prepopulate_id_pref_name|: Pref name for the prepopulated provider ID.
   // |policy_to_pref_map|: Defines the pref names for respective policy keys.
   //     Must contain exactly DEFAULT_SEARCH_KEY_SIZE entries and be ordered
   //     according to the DefaultSearchKey enum.
  DefaultSearchPolicyHandler(
      const char* id_pref_name,
      const char* prepopulate_id_pref_name,
      const PolicyToPreferenceMapEntry policy_to_pref_map[]);
  virtual ~DefaultSearchPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Calls |CheckPolicySettings()| on each of the handlers in |handlers_|
  // and returns whether all of the calls succeeded.
  bool CheckIndividualPolicies(const PolicyMap& policies,
                               PolicyErrorMap* errors);

  // Returns whether there is a value for |policy_name| in |policies|.
  bool HasDefaultSearchPolicy(const PolicyMap& policies,
                              const char* policy_name);

  // Returns whether any default search policies are specified in |policies|.
  bool AnyDefaultSearchPoliciesSpecified(const PolicyMap& policies);

  // Returns whether the default search provider is disabled.
  bool DefaultSearchProviderIsDisabled(const PolicyMap& policies);

  // Returns whether the default search URL is set and valid.  On success, both
  // outparams (which must be non-NULL) are filled with the search URL.
  bool DefaultSearchURLIsValid(const PolicyMap& policies,
                               const Value** url_value,
                               std::string* url_string);

  // Make sure that the |path| is present in |prefs_|.  If not, set it to
  // a blank string.
  void EnsureStringPrefExists(PrefValueMap* prefs, const std::string& path);

  // Make sure that the |path| is present in |prefs_| and is a ListValue.  If
  // not, set it to an empty list.
  void EnsureListPrefExists(PrefValueMap* prefs, const std::string& path);

  // The ConfigurationPolicyHandler handlers for each default search policy.
  std::vector<TypeCheckingPolicyHandler*> handlers_;

  const char* id_pref_name_;
  const char* prepopulate_id_pref_name_;
  const PolicyToPreferenceMapEntry* policy_to_pref_map_;  // weak

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchPolicyHandler);
};

// ConfigurationPolicyHandler for the proxy policies.
class ProxyPolicyHandler : public ConfigurationPolicyHandler {
 public:
  // Constants for the "Proxy Server Mode" defined in the policies.
  // Note that these diverge from internal presentation defined in
  // ProxyPrefs::ProxyMode for legacy reasons. The following four
  // PolicyProxyModeType types were not very precise and had overlapping use
  // cases.
  enum ProxyModeType {
    // Disable Proxy, connect directly.
    PROXY_SERVER_MODE = 0,
    // Auto detect proxy or use specific PAC script if given.
    PROXY_AUTO_DETECT_PROXY_SERVER_MODE = 1,
    // Use manually configured proxy servers (fixed servers).
    PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE = 2,
    // Use system proxy server.
    PROXY_USE_SYSTEM_PROXY_SERVER_MODE = 3,

    MODE_COUNT
  };

  explicit ProxyPolicyHandler(const char* pref_name);
  virtual ~ProxyPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const Value* GetProxyPolicyValue(const PolicyMap& policies,
                                   const char* policy_name);

  // Converts the deprecated ProxyServerMode policy value to a ProxyMode value
  // and places the result in |mode_value|. Returns whether the conversion
  // succeeded.
  bool CheckProxyModeAndServerMode(const PolicyMap& policies,
                                   PolicyErrorMap* errors,
                                   std::string* mode_value);

  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyHandler);
};

// Handles JavaScript policies.
class JavascriptPolicyHandler : public ConfigurationPolicyHandler {
 public:
  explicit JavascriptPolicyHandler(const char* pref_name);
  virtual ~JavascriptPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(JavascriptPolicyHandler);
};

// Handles URLBlacklist policies.
class URLBlacklistPolicyHandler : public ConfigurationPolicyHandler {
 public:
  explicit URLBlacklistPolicyHandler(const char* pref_name);
  virtual ~URLBlacklistPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_name_;
  DISALLOW_COPY_AND_ASSIGN(URLBlacklistPolicyHandler);
};

// Handles RestoreOnStartup policy.
class RestoreOnStartupPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  RestoreOnStartupPolicyHandler(const char* restore_on_startup_pref_name,
                                const char* startup_url_list_pref_name);
  virtual ~RestoreOnStartupPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  void ApplyPolicySettingsFromHomePage(const PolicyMap& policies,
                                       PrefValueMap* prefs);

  const char* restore_on_startup_pref_name_;
  const char* startup_url_list_pref_name_;
  DISALLOW_COPY_AND_ASSIGN(RestoreOnStartupPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
