# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Uninstalls Chrome.

This script reads the uninstall command from registry, calls it, and verifies
the output status code.
"""

import _winreg
import optparse
import subprocess
import sys

import path_resolver


def main():
  parser = optparse.OptionParser(description='Uninstall Chrome.')
  parser.add_option('--system-level', action='store_true', dest='system_level',
                    default=False, help='Uninstall Chrome at system level.')
  options, _ = parser.parse_args()

  # TODO(sukolsak): Add support for uninstalling MSI-based Chrome installs when
  # we support testing MSIs.
  if options.system_level:
    root_key = _winreg.HKEY_LOCAL_MACHINE
  else:
    root_key = _winreg.HKEY_CURRENT_USER
  sub_key = ('SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\'
             '$CHROME_LONG_NAME')
  resolved_sub_key = path_resolver.ResolvePath(sub_key)
  key = _winreg.OpenKey(root_key, resolved_sub_key, 0, _winreg.KEY_QUERY_VALUE)
  uninstall_string, _ = _winreg.QueryValueEx(key, 'UninstallString')
  exit_status = subprocess.call(uninstall_string, shell=True)
  # The exit status for successful uninstallation of Chrome is 19 (see
  # chrome/installer/util/util_constants.h).
  if exit_status != 19:
    raise Exception('Could not uninstall Chrome. The installer exited with '
                    'status %d.' % exit_status)
  return 0

if __name__ == '__main__':
  sys.exit(main())
