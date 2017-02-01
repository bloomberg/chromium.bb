# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser code.

This script currently checks HTML/CSS/JS files in resources/ and ui/webui/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools, and see
http://www.chromium.org/developers/web-development-style-guide for the rules
checked for here.
"""


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def _RunHistogramChecks(input_api, output_api, histogram_name):
  try:
    # Setup sys.path so that we can call histrogram code
    import sys
    original_sys_path = sys.path
    sys.path = sys.path + [input_api.os_path.join(
        input_api.change.RepositoryRoot(),
        'tools', 'metrics', 'histograms')]

    import presubmit_bad_message_reasons
    return presubmit_bad_message_reasons.PrecheckBadMessage(input_api,
      output_api, histogram_name)
  except:
    return [output_api.PresubmitError('Could not verify histogram!')]
  finally:
    sys.path = original_sys_path


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []

  path = input_api.os_path
  cwd = input_api.PresubmitLocalPath()
  resources = path.join(cwd, 'resources')
  webui = path.join(cwd, 'ui', 'webui')

  affected_files = (f.AbsoluteLocalPath() for f in input_api.AffectedFiles())

  would_affect_tests = [
      path.join(cwd, 'PRESUBMIT.py'),
      path.join(cwd, 'test_presubmit.py'),
  ]
  would_affect_tests += input_api.glob(path.join(cwd, 'web_dev_style', '*.py'))

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

    def _vulcanized_resource(p):
      return p.endswith(('vulcanized.html', 'crisper.js'))

    BLACKLIST = [
        'chrome/browser/resources/pdf/index.html',
        'chrome/browser/resources/pdf/index.js'
    ]
    def is_resource(maybe_resource):
      return (maybe_resource.LocalPath() not in BLACKLIST and
          not _vulcanized_resource(maybe_resource.LocalPath()) and
          _html_css_js_resource(maybe_resource.AbsoluteLocalPath()))

    results.extend(resource_checker.ResourceChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(css_checker.CSSChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(html_checker.HtmlChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(js_checker.JSChecker(
        input_api, output_api, file_filter=is_resource).RunChecks())
    results.extend(_RunHistogramChecks(input_api, output_api,
      "BadMessageReasonChrome"))
  finally:
    sys.path = old_path

  return results
