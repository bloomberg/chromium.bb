# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this presubmit check fails or misbehaves, please complain to
# mnissler@chromium.org, pastarmovj@chromium.org or joaodasilva@chromium.org.

import itertools
import sys
import xml.dom.minidom

def _GetPolicyTemplates(template_path):
  # Read list of policies in the template. eval() is used instead of a JSON
  # parser because policy_templates.json is not quite JSON, and uses some
  # python features such as #-comments and '''strings'''. policy_templates.json
  # is actually maintained as a python dictionary.
  with open(template_path) as f:
    template_data = eval(f.read(), {})
  policies = ( policy
               for policy in template_data['policy_definitions']
               if policy['type'] != 'group' )
  groups = ( policy['policies']
             for policy in template_data['policy_definitions']
             if policy['type'] == 'group' )
  subpolicies = ( policy for group in groups for policy in group )
  return list(itertools.chain(policies, subpolicies))

def _CheckPolicyTemplatesSyntax(input_api, output_api):
  local_path = input_api.PresubmitLocalPath()
  filepath = input_api.os_path.join(local_path, 'policy_templates.json')
  if any(f.AbsoluteLocalPath() == filepath
         for f in input_api.AffectedFiles()):
    old_sys_path = sys.path
    try:
      tools_path = input_api.os_path.normpath(
          input_api.os_path.join(local_path, input_api.os_path.pardir, 'tools'))
      sys.path = [ tools_path ] + sys.path
      # Optimization: only load this when it's needed.
      import syntax_check_policy_template_json
      checker = syntax_check_policy_template_json.PolicyTemplateChecker()
      if checker.Run([], filepath) > 0:
        return [output_api.PresubmitError('Syntax error(s) in file:',
                                          [filepath])]
    finally:
      sys.path = old_sys_path
  return []


def _CheckPolicyTestCases(input_api, output_api, policies):
  # Read list of policies in chrome/test/data/policy/policy_test_cases.json.
  root = input_api.change.RepositoryRoot()
  policy_test_cases_file = input_api.os_path.join(
      root, 'chrome', 'test', 'data', 'policy', 'policy_test_cases.json')
  test_names = input_api.json.load(open(policy_test_cases_file)).keys()
  tested_policies = frozenset(name.partition('.')[0]
                              for name in test_names
                              if name[:2] != '--')
  policy_names = frozenset(policy['name'] for policy in policies)

  # Finally check if any policies are missing.
  missing = policy_names - tested_policies
  extra = tested_policies - policy_names
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


def _CheckPolicyHistograms(input_api, output_api, policies):
  root = input_api.change.RepositoryRoot()
  histograms = input_api.os_path.join(
      root, 'tools', 'metrics', 'histograms', 'histograms.xml')
  with open(histograms) as f:
    tree = xml.dom.minidom.parseString(f.read())
  enums = (tree.getElementsByTagName('histogram-configuration')[0]
               .getElementsByTagName('enums')[0]
               .getElementsByTagName('enum'))
  policy_enum = [e for e in enums
                 if e.getAttribute('name') == 'EnterprisePolicies'][0]
  policy_ids = frozenset([int(e.getAttribute('value'))
                          for e in policy_enum.getElementsByTagName('int')])

  error_missing = ('Policy \'%s\' was added to policy_templates.json but not '
                   'to src/tools/metrics/histograms/histograms.xml. '
                   'Please update both files.')
  results = []
  for policy in policies:
    if policy['id'] not in policy_ids:
      results.append(output_api.PresubmitError(error_missing % policy['name']))
  return results


def _CommonChecks(input_api, output_api):
  results = []
  results.extend(_CheckPolicyTemplatesSyntax(input_api, output_api))

  os_path = input_api.os_path
  local_path = input_api.PresubmitLocalPath()
  template_path = os_path.join(local_path, 'policy_templates.json')
  affected_files = input_api.AffectedFiles()
  if any(f.AbsoluteLocalPath() == template_path for f in affected_files):
    try:
      policies = _GetPolicyTemplates(template_path)
    except:
      results.append(output_api.PresubmitError('Invalid Python/JSON syntax.'))
      return results
    results.extend(_CheckPolicyTestCases(input_api, output_api, policies))
    results.extend(_CheckPolicyHistograms(input_api, output_api, policies))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
