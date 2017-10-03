#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs some static analysis checks on Chrome RPM packages using
rpmlint.
"""

import argparse
import os
import subprocess
import sys


SUPPRESSIONS = [
    # Summary message that we would like to filter out.
    'checked;',
    # Google Chrome is not software available on a distro by default,
    # so installing to /opt is correct behavior.
    'dir-or-file-in-opt',
    # Development builds won't be signed.
    'no-signature',
    # The setuid sandbox has permissions 4755.
    'non-standard-executable-perm',
    # The setuid sandbox is a setuid binary.
    'setuid-binary',
    # Some nacl binaries are statically linked but don't have "static"
    # in their name.
    'statically-linked-binary'
]


def line_is_suppressed(line):
  global SUPPRESSIONS
  if line == '':
    return True
  for suppression in SUPPRESSIONS:
    if suppression in line:
      return True
  return False


parser = argparse.ArgumentParser()
parser.add_argument('package', help='path/to/package.rpm')
args = parser.parse_args()
package = os.path.abspath(args.package)

cmd = [
    'rpmlint',
    str(package),
]
rpmlint = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
stdout, _ = rpmlint.communicate()
filtered = '\n'.join(
    [line for line in stdout.split('\n') if not line_is_suppressed(line)])
if filtered != '':
  print filtered
sys.exit(filtered != '')
