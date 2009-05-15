#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


def CheckChangeOnUpload(input_api, output_api):
  return RunUnitTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return (RunUnitTests(input_api, output_api) +
          input_api.canned_checks.CheckDoNotSubmit(input_api, output_api))


def RunUnitTests(input_api, output_api):
  import unittest
  tests_suite = []
  test_loader = unittest.TestLoader()
  def LoadTests(module_name):
    module = __import__(module_name)
    for part in module_name.split('.')[1:]:
      module = getattr(module, part)
    tests_suite.extend(test_loader.loadTestsFromModule(module)._tests)
  # List all the test modules to test here:
  LoadTests('tests.gcl_unittest')
  LoadTests('tests.gclient_test')
  LoadTests('tests.presubmit_unittest')
  LoadTests('tests.trychange_unittest')
  unittest.TextTestRunner(verbosity=0).run(unittest.TestSuite(tests_suite))
  # TODO(maruel): Find a way to block the check-in.
  return []
