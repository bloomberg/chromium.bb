#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import subprocess
import sys


BOT_ASSIGNMENT = {
    'nacl-win32_opt': 'buildbot\\buildbot_win.bat opt 32',
    'nacl-win64_dbg': 'buildbot\\buildbot_win.bat dbg 64',
    'nacl-win64_opt': 'buildbot\\buildbot_win.bat opt 64',
    'nacl-mac10.5_opt': 'bash buildbot/buildbot_mac.sh opt',
    'nacl-mac_opt': 'bash buildbot/buildbot_mac.sh opt',
    'nacl-hardy32_m32_n32_opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'nacl-lucid32_m32_n32_dbg': 'bash buildbot/buildbot_linux.sh dbg 32',
    'nacl-lucid32_m32_n32_opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'nacl-hardy64_m32_n32_opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'nacl-lucid64_m32_n32_opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'nacl-hardy64_m64_n64_opt': 'bash buildbot/buildbot_linux.sh opt 64',
    'nacl-lucid64_m64_n64_dbg': 'bash buildbot/buildbot_linux.sh dbg 64',
    'nacl-lucid64_m64_n64_opt': 'bash buildbot/buildbot_linux.sh opt 64',
    'nacl-arm_opt': 'bash buildbot/buildbot_arm.sh opt',
}


def Main():
  cmd = BOT_ASSIGNMENT.get(os.environ.get('BUILDBOT_BUILDERNAME'))
  if not cmd:
    sys.stderr.write('ERROR - unset/invalid builder name\n')
    sys.exit(1)

  p = subprocess.Popen(cmd, shell=True)
  p.wait()

  sys.exit(p.returncode)


if __name__ == '__main__':
  Main()
