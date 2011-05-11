#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


BOT_ASSIGNMENT = {
    ######################################################################
    # Buildbots.
    ######################################################################
    'xp-newlib-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'xp-glibc-opt': 'buildbot\\buildbot_win.bat opt 32 glibc',
    'vista-64-newlib-dbg': 'buildbot\\buildbot_win.bat dbg 64 newlib',
    'vista-64-newlib-opt': 'buildbot\\buildbot_win.bat opt 64 newlib',
    'win7-64-glibc-dbg': 'buildbot\\buildbot_win.bat dbg 64 glibc',
    'win7-64-glibc-opt': 'buildbot\\buildbot_win.bat opt 64 glibc',
    'xp-bare-newlib-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'win7-32-bare-newlib-opt': 'buildbot\\buildbot_win.bat opt 32 newlib',
    'win7-64-bare-newlib-opt': 'buildbot\\buildbot_win.bat opt 64 newlib',
    'mac10.5-newlib-opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'mac10.6-newlib-opt': 'bash buildbot/buildbot_mac.sh opt 32 newlib',
    'mac10.5-glibc-opt': 'bash buildbot/buildbot_mac.sh opt 32 glibc',
    'mac10.6-glibc-opt': 'bash buildbot/buildbot_mac.sh opt 32 glibc',
    'lucid-32-newlib-dbg': 'bash buildbot/buildbot_linux.sh dbg 32 newlib',
    'lucid-32-newlib-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid-32-glibc-dbg': 'bash buildbot/buildbot_linux.sh dbg 32 glibc',
    'lucid-32-glibc-opt': 'bash buildbot/buildbot_linux.sh opt 32 glibc',
    'lucid-64-newlib-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid-64-newlib-dbg': 'bash buildbot/buildbot_linux.sh dbg 64 newlib',
    'lucid-64-newlib-opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
    'lucid-64-glibc-dbg': 'bash buildbot/buildbot_linux.sh dbg 64 glibc',
    'lucid-64-glibc-opt': 'bash buildbot/buildbot_linux.sh opt 64 glibc',
    'lucid-32-bare-newlib-opt': 'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid-64-bare-newlib-opt': 'bash buildbot/buildbot_linux.sh opt 64 newlib',
     # pnacl bots.

    'pnacl-arm-dbg':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-dbg',
    'pnacl-arm-opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-opt',
    'pnacl-arm-hw-dbg':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-hw-dbg',
    'pnacl-arm-hw-opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-hw-opt',
    'pnacl-x8632':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8632',
    'pnacl-x8664':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8664',
    # Pnacl spec2k bots (obsolete)
    'lucid64-spec-arm': # obsolete
        'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-arm',
    'lucid64-spec-x86': # obsolete
        'bash tests/spec2k/bot_spec.sh 2 ~/cpu2000-redhat64-ia32',
    'lucid64-pnacl-translator': # obsolete
        'bash tests/spec2k/bot_spec.sh 3 ~/cpu2000-redhat64-ia32',
    # Pnacl spec2k bots (obsolete)
    'spec-pnacl-arm':
        'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-arm',
    'spec-pnacl-x8632':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-x8632',
    'spec-pnacl-x8664':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-x8664',
    # NaCl spec2k bot
    'spec-nacl':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-nacl',
    # Valgrind bots.
    'karmic64-valgrind': 'bash buildbot/buildbot_valgrind.sh',
    # Coverage.
    'mac10.6-newlib-coverage': 'bash buildbot/buildbot_coverage_mac.sh',
    'lucid-64-32-newlib-coverage':
        'bash buildbot/buildbot_coverage_linux.sh 32',
    'lucid-64-64-newlib-coverage':
        'bash buildbot/buildbot_coverage_linux.sh 64',
    'xp-newlib-coverage': 'buildbot\\buildbot_coverage_win.bat',
    # PPAPI Integration.
    'lucid64-m32-n32-opt-ppapi':
        'bash buildbot/buildbot_linux.sh opt 32 newlib',
    'lucid64-m64-n64-dbg-ppapi':
        'bash buildbot/buildbot_linux.sh dbg 64 newlib',

    ######################################################################
    # Trybots.
    ######################################################################
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
    # Pnacl scons trybots
    'nacl-arm_opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-try',
    'nacl-arm_hw_opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-hw-try',
    'nacl-lucid64-pnacl1':
        'bash buildbot/buildbot_pnacl1.sh mode-trybot',
    # Pnacl spec2k trybots
    'nacl-lucid64-pnacl2':
        'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-trybot',

    # Toolchain glibc.
    'lucid64-glibc': 'bash buildbot/buildbot_lucid64-glibc-makefile.sh',
    'mac-glibc': 'bash buildbot/buildbot_mac-glibc-makefile.sh',
    'win7-glibc': 'buildbot\\buildbot_windows-glibc-makefile.bat',
    # Toolchain gnu.
    'win7-toolchain_x86': 'buildbot\\buildbot_toolchain_win.bat',
    'mac-toolchain_x86': 'bash buildbot/buildbot_toolchain.sh mac',
    'hardy32-toolchain_x86': 'bash buildbot/buildbot_toolchain.sh linux',
    # Pnacl toolchain builders.
    'linux-armtools-x86_32':
        'bash buildbot/buildbot_toolchain_arm_trusted.sh',
    'linux-pnacl-x86_32':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh',
    'linux-pnacl-x86_64':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh',
    'mac-pnacl-x86_32':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh',

    # Toolchain trybots.
    'nacl-toolchain-lucid64-newlib':
        'bash buildbot/buildbot_toolchain.sh linux',
    'nacl-toolchain-mac-newlib': 'bash buildbot/buildbot_toolchain.sh mac',
    'nacl-toolchain-win7-newlib': 'buildbot\\buildbot_toolchain_win.bat',
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
