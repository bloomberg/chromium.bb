#!/usr/bin/python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to update the test error expectations based on actual results.

This is useful for regenerating test expectations after making changes to the
error format.

To use this run the affected tests, and then pass the input to this script
(either via stdin, or as the first argument). For instance:

  $ ./out/Release/net_unittests --gtest_filter="*VerifyCertificateChain*" | \
     net/data/verify_certificate_chain_unittest/rebase-errors.py

The script will search the unit-test (results.txt in above example) and look
for failure lines and the corresponding actual error string.

It will then go and update the corresponding .pem and .py file.
"""

import common
import glob
import os
import sys
import re


def read_file_to_string(path):
  """Reads a file entirely to a string"""
  with open(path, 'r') as f:
    return f.read()


def write_string_to_file(data, path):
  """Writes a string to a file"""
  print "Writing file %s ..." % (path)
  with open(path, "w") as f:
    f.write(data)


def get_file_paths_for_test(test_name):
  """Returns the file paths (as a tuple) that define a particular unit test.
  For instance given test name 'IntermediateLacksBasicConstraints' it would
  return the paths to:

   * intermediate-lacks-basic-constraints.pem,
   * generate-intermediate-lacks-basic-constraints.py
  """
  # The directory that this python script is stored in.
  base_dir = os.path.dirname(os.path.realpath(__file__))

  # The C++ test name is just a camel case verson of the file name. Rather than
  # converting directly from camel case to a file name, it is simpler to just
  # scan the file list and see which matches. (Not efficient but good enough).
  paths = glob.glob(os.path.join(base_dir, '*.pem'))

  for pem_path in paths:
    file_name = os.path.basename(pem_path)
    file_name_no_extension = os.path.splitext(file_name)[0]

    # Strip the hyphens in file name to bring it closer to the camel case.
    transformed = file_name_no_extension.replace('-', '')

    # Now all that differs is the case.
    if transformed.lower() == test_name.lower():
      py_file_name = 'generate-' + file_name_no_extension + '.py'
      py_path = os.path.join(base_dir, py_file_name)
      return (pem_path, py_path)

  return None


def replace_string(original, start, end, replacement):
  """Replaces the specified range of |original| with |replacement|"""
  return original[0:start] + replacement + original[end:]


def fixup_pem_file(path, actual_errors):
  """Updates the ERRORS block in the test .pem file"""
  contents = read_file_to_string(path)

  # This assumes that ERRORS is the last thing in file, and comes after the
  # VERIFY_RESULT block.
  kEndVerifyResult = '-----END VERIFY_RESULT-----'
  contents = contents[0:contents.index(kEndVerifyResult)]
  contents += kEndVerifyResult
  contents += '\n'
  contents += '\n'
  contents += common.text_data_to_pem('ERRORS', actual_errors)

  # Update the file.
  write_string_to_file(contents, path)


def fixup_py_file(path, actual_errors):
  """Replaces the 'errors = XXX' section of the test's python script"""
  contents = read_file_to_string(path)

  # This assumes that the errors variable uses triple quotes.
  prog = re.compile(r'^errors = """(.*)"""', re.MULTILINE | re.DOTALL)
  result = prog.search(contents)

  # Replace the stuff in between the triple quotes with the actual errors.
  contents = replace_string(contents, result.start(1), result.end(1),
                            actual_errors)

  # Update the file.
  write_string_to_file(contents, path)


def fixup_test(test_name, actual_errors):
  """Updates the test files used by |test_name|, setting the expected error to
  |actual_errors|"""

  # Determine the paths for the corresponding *.pem file and generate-*.py
  pem_path, py_path = get_file_paths_for_test(test_name)

  fixup_pem_file(pem_path, actual_errors)
  fixup_py_file(py_path, actual_errors)


kTestNamePattern = (r'^\[ RUN      \] VerifyCertificateChain/'
                    'VerifyCertificateChainSingleRootTest/0\.(.*)$')
kValueOfLine = 'Value of: errors.ToDebugString()'
kActualPattern = '^  Actual: "(.*)"$'


def main():
  if len(sys.argv) > 2:
    print 'Usage: %s [path-to-unittest-stdout]' % (sys.argv[0])
    sys.exit(1)

  # Read the input either from a file, or from stdin.
  test_stdout = None
  if len(sys.argv) == 2:
    test_stdout = read_file_to_string(sys.argv[1])
  else:
    print 'Reading input from stdin...'
    test_stdout = sys.stdin.read()

  lines = test_stdout.split('\n')

  # Iterate over each line of the unit test stdout.
  for i in range(len(lines) - 3):
    # Figure out the name of the test.
    m = re.search(kTestNamePattern, lines[i])
    if not m:
      continue
    test_name = m.group(1)

    # Confirm that it is a failure having to do with the errors.
    if lines[i + 2] != kValueOfLine:
      continue

    # Get the actual error text (which in gtest output is escaped).
    m = re.search(kActualPattern, lines[i + 3])
    if not m:
      continue
    actual = m.group(1)
    actual = actual.decode('string-escape')

    fixup_test(test_name, actual)


if __name__ == "__main__":
  main()
