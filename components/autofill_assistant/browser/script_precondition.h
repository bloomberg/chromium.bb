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
  static std::unique_ptr<ScriptPrecondition> FromProto(
      const ScriptPreconditionProto& proto);

  ScriptPrecondition(
      const std::vector<std::vector<std::string>>& elements_exist,
      const std::set<std::string>& domain_match,
      std::vector<std::unique_ptr<re2::RE2>> path_pattern,
      const std::vector<ScriptParameterMatchProto>& parameter_match);
  ~ScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|.
  void Check(WebController* web_controller,
             const std::map<std::string, std::string>& parameters,
             base::OnceCallback<void(bool)> callback);

 private:
  void OnCheckElementExists(bool result);
  bool MatchDomain(const GURL& url) const;
  bool MatchPath(const GURL& url) const;
  bool MatchParameters(
      const std::map<std::string, std::string>& parameters) const;

  std::vector<std::vector<std::string>> elements_exist_;
  base::OnceCallback<void(bool)> check_preconditions_callback_;
  size_t pending_elements_exist_check_;

  // Domain (exact match) excluding the last '/' character.
  std::set<std::string> domain_match_;

  // Pattern of the path parts of the URL.
  std::vector<std::unique_ptr<re2::RE2>> path_pattern_;

  // Condition on parameters, identified by name, as found in the intent.
  std::vector<ScriptParameterMatchProto> parameter_match_;

  base::WeakPtrFactory<ScriptPrecondition> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScriptPrecondition);
};

}  // namespace autofill_assistant.

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
