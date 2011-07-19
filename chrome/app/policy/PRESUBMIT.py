# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this presubmit check fails or misbehaves, please complain to
# gfeher@chromium.org or jkummerow@chromium.org.

import sys


def _CommonChecks(input_api, output_api):
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


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
