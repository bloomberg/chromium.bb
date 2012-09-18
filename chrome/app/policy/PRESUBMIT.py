# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this presubmit check fails or misbehaves, please complain to
# mnissler@chromium.org, pastarmovj@chromium.org or joaodasilva@chromium.org.

import itertools
import sys

def _CheckPolicyTemplatesSyntax(input_api, output_api):
  filepath = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                    'policy_templates.json')
  if any(f.AbsoluteLocalPath() == filepath
         for f in input_api.AffectedFiles()):
    old_sys_path = sys.path
    try:
      sys.path = [input_api.PresubmitLocalPath()] + sys.path
      # Optimization: only load this when it's needed.
      import syntax_check_policy_template_json
      checker = syntax_check_policy_template_json.PolicyTemplateChecker()
      if checker.Run([], filepath) > 0:
        return [output_api.PresubmitError('Syntax error(s) in file:',
                                          [filepath])]
    finally:
      sys.path = old_sys_path
  return []


def _CheckPolicyTestCases(input_api, output_api):
  os_path = input_api.os_path
  local_path = input_api.PresubmitLocalPath()
  template_path = os_path.join(local_path, 'policy_templates.json')
  affected_files = input_api.AffectedFiles()
  if not any(f.AbsoluteLocalPath() == template_path for f in affected_files):
    return []

  # Read list of policies in the template. eval() is used instead of a JSON
  # parser because policy_templates.json is not quite JSON, and uses some
  # python features such as #-comments and '''strings'''. policy_templates.json
  # is actually maintained as a python dictionary.
  with open(template_path) as f:
    template_data = eval(f.read(), {})
  policies = ( policy['name']
               for policy in template_data['policy_definitions']
               if policy['type'] != 'group' )
  groups = ( policy['policies']
             for policy in template_data['policy_definitions']
             if policy['type'] == 'group' )
  subpolicies = ( policy['name'] for group in groups for policy in group )
  template_policies = frozenset(itertools.chain(policies, subpolicies))

  # Read list of policies in chrome/test/data/policy/policy_test_cases.json.
  root = input_api.change.RepositoryRoot()
  policy_test_cases_file = os_path.join(
      root, 'chrome', 'test', 'data', 'policy', 'policy_test_cases.json')
  test_names = input_api.json.load(open(policy_test_cases_file)).keys()
  tested_policies = frozenset(
      [name for name in test_names if name[:2] != '--'])

  # Finally check if any policies are missing.
  missing = template_policies - tested_policies
  extra = tested_policies - template_policies
  error_missing = ('Policy \'%s\' was added to policy_templates.json but not '
                   'to src/chrome/test/data/policy/policy_test_cases.json. '
                   'Please update both files.')
  error_extra = ('Policy \'%s\' is tested by '
                 'src/chrome/test/data/policy/policy_test_cases.json but is not'
                 ' defined in policy_templates.json. Please update both files.')
  results = []
  for policy in missing:
    results.append(output_api.PresubmitError(error_missing % policy))
  for policy in extra:
    results.append(output_api.PresubmitError(error_extra % policy))
  return results


def _CommonChecks(input_api, output_api):
  results = []
  results.extend(_CheckPolicyTemplatesSyntax(input_api, output_api))
  results.extend(_CheckPolicyTestCases(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
