// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill_assistant {

// Class represents a set of preconditions for a script to be executed.
class ScriptPrecondition {
 public:
  // Builds a precondition from its proto representation. Returns nullptr if the
  // preconditions are invalid.
  //
  // Note: The |script_path| paramter is used to allow logging misconfigured
  // scripts.
  static std::unique_ptr<ScriptPrecondition> FromProto(
      const std::string& script_path,
      const ScriptPreconditionProto& script_precondition_proto);

  ScriptPrecondition(
      const std::vector<std::vector<std::string>>& elements_exist,
      const std::set<std::string>& domain_match,
      std::vector<std::unique_ptr<re2::RE2>> path_pattern,
      const std::vector<ScriptParameterMatchProto>& parameter_match,
      const std::vector<FormValueMatchProto>& form_value_match,
      const std::vector<ScriptStatusMatchProto>& status_match);
  ~ScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|.
  void Check(WebController* web_controller,
             const std::map<std::string, std::string>& parameters,
             const std::map<std::string, ScriptStatusProto>& executed_scripts,
             base::OnceCallback<void(bool)> callback);

 private:
  void OnCheckElementExists(bool result);
  bool MatchDomain(const GURL& url) const;
  bool MatchPath(const GURL& url) const;
  bool MatchParameters(
      const std::map<std::string, std::string>& parameters) const;
  bool MatchScriptStatus(
      const std::map<std::string, ScriptStatusProto>& executed_scripts) const;

  void OnGetFieldValue(const std::string& value);

  // Return if all checks have been completed and we have not returned anything
  // yet.
  void MaybeRunCheckPreconditionCallback();

  std::vector<std::vector<std::string>> elements_exist_;
  base::OnceCallback<void(bool)> check_preconditions_callback_;
  size_t pending_preconditions_check_count_;
  bool all_preconditions_check_satisfied_;

  // Domain (exact match) excluding the last '/' character.
  std::set<std::string> domain_match_;

  // Pattern of the path parts of the URL.
  std::vector<std::unique_ptr<re2::RE2>> path_pattern_;

  // Condition on parameters, identified by name, as found in the intent.
  std::vector<ScriptParameterMatchProto> parameter_match_;

  // Conditions on form fields value.
  std::vector<FormValueMatchProto> form_value_match_;

  // Conditions regarding the execution status of passed scripts.
  std::vector<ScriptStatusMatchProto> status_match_;

  base::WeakPtrFactory<ScriptPrecondition> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScriptPrecondition);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
