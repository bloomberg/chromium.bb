# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser code.

This script currently only checks HTML/CSS/JS files in resources/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl/git cl, and see
http://www.chromium.org/developers/web-development-style-guide for the rules
checked for here.
"""


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []

  path = input_api.os_path
  cwd = input_api.PresubmitLocalPath()
  resources = path.join(cwd, 'resources')
  webui = path.join(cwd, 'ui', 'webui')

  affected_files = (f.AbsoluteLocalPath() for f in input_api.AffectedFiles())
  would_affect_tests = (
      path.join(cwd, 'PRESUBMIT.py'),
      path.join(cwd, 'test_presubmit.py'),
      path.join(cwd, 'web_dev_style', 'css_checker.py'),
      path.join(cwd, 'web_dev_style', 'html_checker.py'),
      path.join(cwd, 'web_dev_style', 'js_checker.py'),
  )
  if any(f for f in affected_files if f in would_affect_tests):
    tests = [path.join(cwd, 'test_presubmit.py')]
    results.extend(
        input_api.canned_checks.RunUnitTests(input_api, output_api, tests))

  import sys
  old_path = sys.path

  try:
    sys.path = [cwd] + old_path
    from web_dev_style import (resource_checker, css_checker, html_checker,
                               js_checker)

    search_dirs = (resources, webui)
    def _html_css_js_resource(p):
      return p.endswith(('.html', '.css', '.js')) and p.startswith(search_dirs)

    BLACKLIST = ['chrome/browser/resources/pdf/index.html',
                 'chrome/browser/resources/pdf/index.js']
    def is_resource(maybe_resource):
      return (maybe_resource.LocalPath() not in BLACKLIST and
          _html_css_js_resource(maybe_resource.AbsoluteLocalPath()))

    results.extend(resource_checker.ResourceChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(css_checker.CSSChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(html_checker.HtmlChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(js_checker.JSChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
  finally:
    sys.path = old_path

  return results
