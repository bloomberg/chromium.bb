# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  results = []
  import sys
  if not sys.version.startswith('2.5'):
    # Depot_tools has the particularity that it needs to be tested on python
    # 2.5. But we don't want the presubmit check to fail if it is not installed.
    results.append(output_api.PresubmitNotifyResult(
        'You should install python 2.5 and run ln -s $(which python2.5) python.'
        '\n'
        'A great place to put this symlink is in depot_tools.\n'
        'Otherwise, you break depot_tools on python 2.5, you get to keep the '
        'pieces.'))


  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  black_list = list(input_api.DEFAULT_BLACK_LIST) + [
      r'^cpplint\.py$',
      r'^python_bin[\/\\].+',
      r'^svn_bin[\/\\].+',
      r'^tests[\/\\]\w+?[\/\\].+']
  results.extend(input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      white_list=[r'.*\.py$'],
      black_list=black_list))

  # TODO(maruel): Make sure at least one file is modified first.
  # TODO(maruel): If only tests are modified, only run them.
  results.extend(input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      'tests',
      whitelist=[r'.*test\.py$']))
  results.extend(RunGitClTests(input_api, output_api))
  return results


def RunGitClTests(input_api, output_api):
  """Run all the shells scripts in the directory test.
  """
  if input_api.platform == 'win32':
    # Skip for now as long as the test scripts are bash scripts.
    return []

  # First loads a local Rietveld instance.
  import sys
  old_sys_path = sys.path
  try:
    sys.path = [input_api.PresubmitLocalPath()] + sys.path
    from tests import local_rietveld  # pylint: disable=W0403
    server = local_rietveld.LocalRietveld()
  finally:
    sys.path = old_sys_path

  results = []
  try:
    # Start a local rietveld instance to test against.
    server.start_server()
    test_path = input_api.os_path.abspath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'tests'))
    for test in input_api.os_listdir(test_path):
      # test-lib.sh is not an actual test so it should not be run. The other
      # tests are tests known to fail.
      DISABLED_TESTS = (
          'owners.sh', 'push-from-logs.sh', 'rename.sh', 'test-lib.sh')
      if test in DISABLED_TESTS or not test.endswith('.sh'):
        continue

      print('Running %s' % test)
      try:
        if input_api.verbose:
          input_api.subprocess.check_call(
              [input_api.os_path.join(test_path, test)], cwd=test_path)
        else:
          input_api.subprocess.check_output(
              [input_api.os_path.join(test_path, test)], cwd=test_path)
      except (OSError, input_api.subprocess.CalledProcessError), e:
        results.append(output_api.PresubmitError('%s failed\n%s' % (test, e)))
  except local_rietveld.Failure, e:
    results.append(output_api.PresubmitError('\n'.join(str(i) for i in e.args)))
  finally:
    server.stop_server()
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(CommonChecks(input_api, output_api))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(
      input_api,
      output_api))
  return output
