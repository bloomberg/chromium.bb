#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for running XR browser tests properly.

The XR browser tests require some additional setup to run properly, for example
setting ACLs on Windows in order to allow the sandboxed process to work
properly. This setup can technically be done in the test itself, but can take
a significant amount of time, causing tests to time out.

This could be worked around by changing the test runner being used from
//chrome/test/base/browser_tests_runner.cc to a custom one that performs the
setup outside of any tests, but that duplicates code and could potentially make
the XR tests miss any changes made to the base file.
"""

import argparse
import logging
import os
import subprocess
import sys


# These ACLs allow the app container users (the really long numerical strings)
# to read and execute files in whatever directory they're applied to (in this
# case, the output directory). This is required in order for the isolated XR
# device service to work in a sandboxed process. These would normally be set
# during installation, but have to be manually set for tests since the installer
# isn't used.
ACL_STRINGS = [
  'S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-4240214049-4028510897-3317428798:(OI)(CI)(RX)',
  'S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-316187997-3786583170-1043257646:(OI)(CI)(RX)',
]


def ResetACLs(path):
  logging.warning('Setting ACLs on %s to default.', path)
  try:
    # It's normally fine to inherit the ACLs from parents, but in this case,
    # we need to explicitly reset every file via /t.
    _ = subprocess.check_output(
        ['icacls', path, '/reset', '/t'], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Failed to reset ACLs on path %s', path)
    logging.error('Command output: %s', e.output)
    sys.exit(e.returncode)

def SetupWindowsACLs():
  # This should be copied into the root of the output directory, so the
  # directory this file is in should be the directory we want to change the
  # ACLs of.
  acl_dir = os.path.abspath(os.path.dirname(__file__))
  try:
    existing_acls = subprocess.check_output(
        ['icacls', acl_dir], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Failed to retrieve existing ACLs for directory %s', acl_dir)
    logging.error('Command output: %s', e.output)
    sys.exit(e.returncode)

  have_reset = False
  for acl in ACL_STRINGS:
    if acl not in existing_acls:
      if not have_reset:
        # Some bots seem to have explicit ACLs set on the isolated input
        # directory. Most still have the default, so it appears to be leftover
        # from some previous thing and no longer actively used. So, if we
        # haven't set our ACLs before on this directory, reset them to default
        # first since the explicit ones can cause issues with ours not being
        # inherited.
        have_reset = True
        ResetACLs(acl_dir)
      try:
        logging.warning(
            'ACL %s not set on output directory. Attempting to set.', acl)
        _ = subprocess.check_output(['icacls', acl_dir, '/grant', '*%s' % acl],
            stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError as e:
        logging.error('Failed to apply ACL %s to directory %s', acl, acl_dir)
        logging.error('Command output: %s', e.output)
        sys.exit(e.returncode)


def main():
  parser = argparse.ArgumentParser()
  _, rest_args = parser.parse_known_args()

  test_executable = 'xr_browser_tests_binary'
  if sys.platform == 'win32':
    SetupWindowsACLs()
    test_executable += '.exe'

  test_executable = os.path.abspath(
      os.path.join(os.path.dirname(__file__), test_executable))
  subprocess.call(
      [test_executable, '--run-through-xr-wrapper-script'] + rest_args)

if __name__ == '__main__':
  main()