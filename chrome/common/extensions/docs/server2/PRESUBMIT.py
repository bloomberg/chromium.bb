# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting extensions docs server

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import sys
sys.path.insert(0, '.')
import build_server
build_server.main()
sys.path.pop(0)

WHITELIST = [ r'.+_test.py$' ]
# The integration tests are selectively run from the PRESUBMIT in
# chrome/common/extensions.
BLACKLIST = [ r'integration_test.py$' ]

def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', whitelist=WHITELIST, blacklist=BLACKLIST)

def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', whitelist=WHITELIST, blacklist=BLACKLIST)
