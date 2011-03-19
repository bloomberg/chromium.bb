#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import subprocess
import sys


BOT_ASSIGNMENT = {
    # Buildbots.
    'xp-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32',
    'vista64-m64-n64-dbg': 'buildbot\\buildbot_win.bat dbg 64',
    'vista64-m64-n64-opt': 'buildbot\\buildbot_win.bat opt 64',
    'xp-bare-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32',
    'win7-bare-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32',
    'win7-bare-m64-n64-dbg': 'buildbot\\buildbot_win.bat dbg 64',
    'win7-bare-m64-n64-opt': 'buildbot\\buildbot_win.bat opt 64',
    'mac10.5-m32-n32-opt': 'bash buildbot/buildbot_mac.sh opt',
    'mac10.6-m32-n32-opt': 'bash buildbot/buildbot_mac.sh opt',
    'hardy32-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'hardy64-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'hardy64-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64',
    'lucid32-m32-n32-dbg': 'bash buildbot/buildbot_linux.sh dbg 32',
    'lucid32-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'lucid64-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'lucid64-m64-n64-dbg': 'bash buildbot/buildbot_linux.sh dbg 64',
    'lucid64-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64',
    'lucid32-bare-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32',
    'lucid64-bare-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64',
    'hardy64-marm-narm-dbg': 'bash buildbot/buildbot_arm.sh dbg',
    'hardy64-marm-narm-opt': 'bash buildbot/buildbot_arm.sh opt',
    'arm-marm-narm-test-dbg': 'bash buildbot/buildbot_arm_hw.sh dbg',
    'arm-marm-narm-test-opt': 'bash buildbot/buildbot_arm_hw.sh opt',
    'lucid64-pnacl1': 'bash buildbot/buildbot_linux.sh opt 64',
    'lucid64-pnacl2': 'bash buildbot/buildbot_linux.sh opt 64',

    # Trybots.
    'nacl-arm_hw_opt': 'bash buildbot/buildbot_arm_hw.sh opt',
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
    'nacl-lucid64-pnacl1': 'bash buildbot/buildbot_pnacl1.sh',
    'nacl-lucid64-pnacl2': 'bash buildbot/buildbot_pnacl2.sh',

    # Toolchain.
    'lucid64-glibc': 'bash buildbot/buildbot_lucid64-glibc-makefile.sh',
    'mac-glibc': 'bash buildbot/buildbot_mac-glibc-makefile.sh',
    'win7-glibc': 'buildbot\\buildbot_windows-glibc-makefile.bat',

    # Toolchain trybots.
    'nacl-lucid64-glibc': 'bash buildbot/buildbot_lucid64-glibc-makefile.sh',
    'nacl-mac-glibc': 'bash buildbot/buildbot_mac-glibc-makefile.sh',
    'nacl-win7-glibc': 'buildbot\\buildbot_windows-glibc-makefile.bat',
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
