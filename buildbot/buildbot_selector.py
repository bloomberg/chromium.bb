#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


BOT_ASSIGNMENT = {
    # Buildbots.
    'xp-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'vista64-m64-n64-dbg': 'buildbot\\buildbot_win.bat dbg 64 newlib',
    'vista64-m64-n64-opt': 'buildbot\\buildbot_win.bat opt 64 newlib',
    'xp-bare-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'win7-bare-m32-n32-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'win7-bare-m64-n64-dbg': 'buildbot\\buildbot_win.bat dbg 64 newlib',
    'win7-bare-m64-n64-opt': 'buildbot\\buildbot_win.bat opt 64 newlib',
    'win7atom64-m64-n64-test-dbg': 'buildbot\\buildbot_win_atom.bat dbg 64',
    'win7atom64-m64-n64-test-opt': 'buildbot\\buildbot_win_atom.bat opt 64',
    'mac10.5-m32-n32-opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'mac10.6-m32-n32-opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'hardy32-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'hardy64-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'hardy64-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'lucid32-m32-n32-dbg': 'bash buildbot/buildbot_linux.sh dbg 32 newlib',
    'lucid32-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid64-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid64-m64-n64-dbg': 'bash buildbot/buildbot_linux.sh dbg 64 newlib',
    'lucid64-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'lucid32-bare-m32-n32-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid64-bare-m64-n64-opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'hardy64-marm-narm-dbg':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-dbg',
    'hardy64-marm-narm-opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-opt',
    'arm-marm-narm-test-dbg': 'bash buildbot/buildbot_arm_hw.sh dbg',
    'arm-marm-narm-test-opt': 'bash buildbot/buildbot_arm_hw.sh opt',
    'lucid64-pnacl1': 'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8632',
    'lucid64-pnacl2': 'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8664',
    'karmic64-valgrind': 'bash buildbot/buildbot_valgrind.sh',
    # Coverage.
    'mac-m32-n32-coverage': 'bash buildbot/buildbot_coverage_mac.sh',
    'hardy64-m32-n32-coverage': 'bash buildbot/buildbot_coverage_linux.sh 32',
    'hardy64-m64-n64-coverage': 'bash buildbot/buildbot_coverage_linux.sh 64',
    'hardy64-marm-narm-coverage': 'bash buildbot/buildbot_coverage_arm.sh',
    'xp-m32-n32-coverage': 'buildbot\\buildbot_coverage_win.bat',

    # Trybots.
    'nacl-arm_hw_opt': 'bash buildbot/buildbot_arm_hw.sh opt',
    'nacl-win32_newlib_opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'nacl-win32_glibc_opt': 'buildbot\\buildbot_win.bat opt 32 glibc',
    'nacl-win64_newlib_dbg': 'buildbot\\buildbot_win.bat dbg 64 newlib',
    'nacl-win64_newlib_opt': 'buildbot\\buildbot_win.bat opt 64 newlib',
    'nacl-win64_glibc_opt': 'buildbot\\buildbot_win.bat opt 64 glibc',
    'nacl-mac10.5_newlib_opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'nacl-mac10.5_glibc_opt': 'bash buildbot/buildbot_mac.sh opt 32 glibc',
    'nacl-mac10.6_newlib_opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'nacl-mac10.6_glibc_opt': 'bash buildbot/buildbot_mac.sh opt 32 glibc',
    'nacl-hardy32_newlib_opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'nacl-lucid32_newlib_dbg': 'bash buildbot/buildbot_linux.sh dbg 32 newlib',
    'nacl-lucid32_newlib_opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'nacl-lucid32_glibc_opt': 'bash buildbot/buildbot_linux.sh opt 32 glibc',
    'nacl-hardy64_newlib_opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'nacl-lucid64_newlib_dbg': 'bash buildbot/buildbot_linux.sh dbg 64 newlib',
    'nacl-lucid64_newlib_opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'nacl-lucid64_glibc_opt': 'bash buildbot/buildbot_linux.sh opt 64 glibc',
    'nacl-arm_opt': 'bash buildbot/buildbot_arm.sh opt',
    'nacl-lucid64-pnacl1': 'bash buildbot/buildbot_pnacl1.sh mode-trybot',
    'nacl-lucid64-pnacl2': 'bash buildbot/buildbot_pnacl2.sh',

    # Toolchain glibc.
    'lucid64-glibc': 'bash buildbot/buildbot_lucid64-glibc-makefile.sh',
    'mac-glibc': 'bash buildbot/buildbot_mac-glibc-makefile.sh',
    'win7-glibc': 'buildbot\\buildbot_windows-glibc-makefile.bat',
    # Toolchain gnu.
    'win7-toolchain_x86': 'buildbot\\buildbot_toolchain_win.bat',
    'mac-toolchain_x86': 'bash buildbot/buildbot_toolchain.sh mac',
    'hardy32-toolchain_x86': 'bash buildbot/buildbot_toolchain.sh linux',
    # Toolchain pnacl.
    'hardy32-toolchain_arm-trusted':
        'bash buildbot/buildbot_toolchain_arm_trusted.sh',
    'hardy64-toolchain_arm-untrusted':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh',
    'lucid32-toolchain_arm-untrusted':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh',

    # Toolchain trybots.
    'nacl-toolchain-lucid64-glibc':
        'bash buildbot/buildbot_lucid64-glibc-makefile.sh',
    'nacl-toolchain-mac-glibc': 'bash buildbot/buildbot_mac-glibc-makefile.sh',
    'nacl-toolchain-win7-glibc':
        'buildbot\\buildbot_windows-glibc-makefile.bat',
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
