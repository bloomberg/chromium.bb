# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium WebUI resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl/git cl, and see
http://www.chromium.org/developers/web-development-style-guide for the rules
we're checking against here.
"""


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  resources = input_api.PresubmitLocalPath()

  path = input_api.os_path
  affected_files = (f.AbsoluteLocalPath() for f in input_api.AffectedFiles())
  would_affect_tests = (
      path.join(resources, 'PRESUBMIT.py'),
      path.join(resources, 'test_presubmit.py'),
      path.join(resources, 'web_dev_style', 'css_checker.py'),
      path.join(resources, 'web_dev_style', 'js_checker.py'),
  )
  if any(f for f in affected_files if f in would_affect_tests):
    tests = [path.join(resources, 'test_presubmit.py')]
    results.extend(
        input_api.canned_checks.RunUnitTests(input_api, output_api, tests))

  import sys
  old_path = sys.path

  try:
    sys.path = [resources] + old_path
    from web_dev_style import css_checker, js_checker

    def _html_css_js_resource(p):
      return p.endswith(('.html', '.css', '.js')) and p.startswith(resources)

    def is_resource(maybe_resource):
      return _html_css_js_resource(maybe_resource.AbsoluteLocalPath())

    # TODO(samarth): remove this exception <http://crbug.com/222642>.
    instant_path = path.join(resources, 'local_omnibox_popup')
    def is_resource_no_instant(maybe_resource):
      f = maybe_resource.AbsoluteLocalPath()
      return _html_css_js_resource(f) and not f.startswith(instant_path)

    results.extend(css_checker.CSSChecker(
        input_api, output_api, file_filter=is_resource_no_instant).RunChecks())
    results.extend(js_checker.JSChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
  finally:
    sys.path = old_path

  return results
