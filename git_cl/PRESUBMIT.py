# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


def CheckChangeOnUpload(input_api, output_api):
  return RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunTests(input_api, output_api)


def RunTests(input_api, output_api):
  """Run all the shells scripts in the directory test.
  """
  # Not exposed from InputApi.
  from os import listdir

  # First loads a local Rietveld instance.
  import sys
  old_sys_path = sys.path
  try:
    sys.path = [input_api.PresubmitLocalPath()] + sys.path
    from test import local_rietveld  # pylint: disable=W0403
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
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'test'))
    for test in listdir(test_path):
      # push-from-logs and rename fails for now. Remove from this list once
      # they work.
      if (test in ('push-from-logs.sh', 'rename.sh', 'test-lib.sh') or
          not test.endswith('.sh')):
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
