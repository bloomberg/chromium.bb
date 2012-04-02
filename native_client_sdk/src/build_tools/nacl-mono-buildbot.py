#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import buildbot_common
import build_utils


def main(args):
  args = args[1:]

  buildbot_revision = os.environ.get('BUILDBOT_REVISION', '')
  assert buildbot_revision
  sdk_revision = buildbot_revision.split(':')[0]
  pepper_revision = build_utils.ChromeMajorVersion()

  install_dir = 'naclmono'
  buildbot_common.RemoveDir(install_dir)

  buildbot_common.Run([sys.executable, 'nacl-mono-builder.py',
                      '--arch', 'x86-32', '--install-dir', install_dir] + args)
  buildbot_common.Run([sys.executable, 'nacl-mono-builder.py',
                      '--arch', 'x86-64', '--install-dir', install_dir] + args)
  buildbot_common.Run([sys.executable, 'nacl-mono-archive.py',
                      '--sdk-revision', sdk_revision,
                      '--pepper-revision', pepper_revision,
                      '--install-dir', install_dir] + args)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
