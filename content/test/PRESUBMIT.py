# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""test_expectations.txt presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import os
import sys

TEST_EXPECTATIONS_FILENAMES = ['test_expectations.txt', 'TestExpectations']

def LintTestFiles(input_api, output_api):
  current_dir = str(input_api.PresubmitLocalPath())
  tools_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  src_dir = os.path.dirname(tools_dir)

  subproc = input_api.subprocess.Popen(
      [input_api.python_executable,
       input_api.os.path.join(src_dir, 'third_party', 'WebKit', 'Tools',
           'Scripts', 'lint-test-expectations')],
      stdin=input_api.subprocess.PIPE,
      stdout=input_api.subprocess.PIPE,
      stderr=input_api.subprocess.STDOUT)
  stdout_data = subproc.communicate()[0]
  is_error = lambda line: (input_api.re.match('^Line:', line) or
                           input_api.re.search('ERROR Line:', line))
  error = filter(is_error, stdout_data.splitlines())
  if error:
    return [output_api.PresubmitError('Lint error\n%s' % '\n'.join(error),
                                      long_text=stdout_data)]
  return []

def LintTestExpectations(input_api, output_api):
  for path in input_api.LocalPaths():
    if input_api.os_path.basename(path) in TEST_EXPECTATIONS_FILENAMES:
      return LintTestFiles(input_api, output_api)
  return []


def CheckChangeOnUpload(input_api, output_api):
  return LintTestExpectations(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return LintTestExpectations(input_api, output_api)
