#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Auto-generates the WebGL conformance test list header file.

Parses the WebGL conformance test *.txt file, which contains a list of URLs
for individual conformance tests (each on a new line). It recursively parses
*.txt files. For each test URL, the matching gtest call is created and
sent to the C++ header file.
"""

import getopt
import os
import re
import sys

COPYRIGHT = """\
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""
WARNING = """\
// This file is auto-generated from a script. DO NOT EDIT!
// It is included by webgl_conformance_tests.cc

"""
HEADER_GUARD = """\
#ifndef CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_
#define CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_

"""
HEADER_GUARD_END = """
#endif  // CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_
"""

# Assume this script is run from the src/chrome/ directory.
INPUT_DIR = "../third_party/webgl_conformance"
INPUT_FILE = "00_test_list.txt"
EXPECTATION_FILE = "test/gpu/webgl_conformance_test_expectations.txt"
EXPECTATION_REGEXP = re.compile(
    r'^(?P<BUG>\S+)\s+'
     '(?P<OS>(\s*(WIN|MAC|LINUX)\s*)+):'
     '(?P<TEST>[^=]+)='
     '(?P<OUTCOME>(\s*(PASS|FAIL|TIMEOUT)\s*)+)')

def is_matching_os(expected_os_list):
  """Returns true if the current OS is in the given list.

  Given a list containing 'WIN', 'MAC' or 'LINUX', return true if the current
  OS, represented as 'win32', 'darwin' or 'linux*', respectively, exists in the
  list.
  """
  if sys.platform.startswith('linux') and 'LINUX' in expected_os_list:
    return True;
  if sys.platform == 'darwin' and 'MAC' in expected_os_list:
    return True;
  if sys.platform == 'win32' and 'WIN' in expected_os_list:
    return True;
  return False;

def main(argv):
  """Main function for the WebGL conformance test list generator.

  If "-i" or "--input" is specified, the parsed *.txt files are printed to
  standard output and no header file is generated. In this case, "output"
  is os.devnull, meaning all output.write() calls are NOPs.
  """
  try:
    opts, args = getopt.getopt(argv, "i", "input")
  except getopt.GetoptError, msg:
    print >> sys.stderr, str(msg)
    return 1

  print_input_files = False
  for opt, value in opts:
    if opt in ("-i", "--input"):
      print_input_files = True

  if print_input_files:
    output_file = os.devnull
  elif len(args) == 1:
    output_file = args[0]
  else:
    print >> sys.stderr, "ERROR: Specify a single output header file."
    return 1

  if not os.path.exists(os.path.join(INPUT_DIR, INPUT_FILE)):
    print >> sys.stderr, "ERROR: WebGL conformance tests do not exist."
    return 1

  test_prefix = {}
  if os.path.exists(EXPECTATION_FILE):
    test_expectations = open(EXPECTATION_FILE)
    for line in test_expectations:
      line_match = EXPECTATION_REGEXP.match(line)
      if line_match:
        match_dict = line_match.groupdict()
        os_list = match_dict['OS'].strip().split()
        if not is_matching_os(os_list):
          continue
        test = match_dict['TEST'].strip()
        outcome_list = match_dict['OUTCOME'].strip().split()
        if 'TIMEOUT' in outcome_list:
          test_prefix[test] = "DISABLED_"
        elif 'FAIL' in outcome_list:
          if 'PASS' in outcome_list:
            test_prefix[test] = "FLAKY_"
          else:
            test_prefix[test] = "FAILS_"
    test_expectations.close()

  output = open(output_file, "w")
  output.write(COPYRIGHT)
  output.write(WARNING)
  output.write(HEADER_GUARD)

  unparsed_files = [INPUT_FILE]
  while unparsed_files:
    filename = unparsed_files.pop(0)
    try:
      input = open(os.path.join(INPUT_DIR, filename))
    except IOError:
      print >> sys.stderr, "WARNING: %s does not exist (skipped)." % filename
      continue

    if print_input_files:
      print "%s/%s" % (INPUT_DIR, filename)
    for url in input:
      url = re.sub("//.*", "", url)
      url = re.sub("#.*", "", url)
      url = re.sub("\s+", "", url)

      # Queue all text files for parsing, because test list URLs are nested
      # through .txt files.
      if re.match(".+\.txt\s*$", url):
        unparsed_files.append(url)

      # Convert the filename to a valid test name and output the gtest code.
      elif not print_input_files and url:
        name = os.path.splitext(url)[0]
        name = re.sub("\W+", "_", name)
        # Cannot use os.path.join() because Windows with use "\\" but this path
        # is sent through javascript.
        url = "%s/%s" % (os.path.dirname(filename), url)
        if os.path.exists(os.path.join(INPUT_DIR, url)):
          # Append "DISABLED_" or "FAILS_" if needed.
          if name in test_prefix:
            name = test_prefix[name] + name
          output.write('CONFORMANCE_TEST(%s,\n  "%s");\n' % (name, url))
        else:
          print >> sys.stderr, "WARNING: %s does not exist (skipped)." % url
    input.close()

  output.write(HEADER_GUARD_END)
  output.close()
  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
