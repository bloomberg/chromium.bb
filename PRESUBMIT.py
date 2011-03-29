# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

UNIT_TESTS = [
  'tests.fix_encoding_test',
  'tests.gcl_unittest',
  'tests.gclient_scm_test',
  'tests.gclient_smoketest',
  'tests.gclient_utils_test',
  'tests.owners_unittest',
  'tests.presubmit_unittest',
  'tests.scm_unittest',
  'tests.trychange_unittest',
  'tests.watchlists_unittest',
]


def CommonChecks(input_api, output_api):
  output = []
  # Verify that LocalPath() is local, e.g.:
  #   os.path.join(PresubmitLocalPath(), LocalPath()) == AbsoluteLocalPath()
  for i in input_api.AffectedFiles():
    if (input_api.os_path.join(input_api.PresubmitLocalPath(), i.LocalPath()) !=
        i.AbsoluteLocalPath()):
      output.append(output_api.PresubmitError('Path inconsistency'))
      # Return right away because it needs to be fixed first.
      return output

  output.extend(input_api.canned_checks.CheckOwners(input_api, output_api))

  output.extend(input_api.canned_checks.RunPythonUnitTests(
      input_api,
      output_api,
      UNIT_TESTS))

  white_list = [r'.*\.py$', r'^git-try$']
  black_list = list(input_api.DEFAULT_BLACK_LIST) + [
      r'^cpplint\.py$',
      r'^tests[\/\\]\w+?[\/\\].+',
      ]
  output.extend(input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      white_list=white_list,
      black_list=black_list))
  output.extend(RunGitClTests(input_api, output_api))
  return output


def RunGitClTests(input_api, output_api):
  """Run all the shells scripts in the directory test.
  """
  # Not exposed from InputApi.
  from os import listdir

  # First loads a local Rietveld instance.
  import sys
  old_sys_path = sys.path
  try:
    sys.path = [input_api.PresubmitLocalPath()] + sys.path
    from tests import local_rietveld  # pylint: disable=W0403
    server = local_rietveld.LocalRietveld()
  finally:
    sys.path = old_sys_path

  # Set to True for testing.
  verbose = False
  if verbose:
    stdout = None
    stderr = None
  else:
    stdout = input_api.subprocess.PIPE
    stderr = input_api.subprocess.STDOUT
  output = []
  try:
    # Start a local rietveld instance to test against.
    server.start_server()
    test_path = input_api.os_path.abspath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'tests'))
    for test in listdir(test_path):
      # test-lib.sh is not an actual test so it should not be run. The other
      # tests are tests known to fail.
      DISABLED_TESTS = (
          'owners.sh', 'push-from-logs.sh', 'rename.sh', 'test-lib.sh')
      if test in DISABLED_TESTS or not test.endswith('.sh'):
        continue

      print('Running %s' % test)
      proc = input_api.subprocess.Popen(
          [input_api.os_path.join(test_path, test)],
          cwd=test_path,
          stdout=stdout,
          stderr=stderr)
      proc.communicate()
      if proc.returncode != 0:
        output.append(output_api.PresubmitError('%s failed' % test))
  except local_rietveld.Failure, e:
    output.append(output_api.PresubmitError('\n'.join(str(i) for i in e.args)))
  finally:
    server.stop_server()
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(CommonChecks(input_api, output_api))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(
      input_api,
      output_api))
  return output
