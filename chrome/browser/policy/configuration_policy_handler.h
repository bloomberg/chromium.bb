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

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
