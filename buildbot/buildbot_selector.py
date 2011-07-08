#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

python = sys.executable

BOT_ASSIGNMENT = {
    ######################################################################
    # Buildbots.
    ######################################################################
    'xp-newlib-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 newlib',
    'xp-glibc-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 glibc',
    'vista-64-newlib-dbg':
        python + ' buildbot\\buildbot_standard.py dbg 64 newlib',
    'vista-64-newlib-opt':
        python + ' buildbot\\buildbot_standard.py opt 64 newlib',
    'win7-64-glibc-dbg':
        python + ' buildbot\\buildbot_standard.py dbg 64 glibc',
    'win7-64-glibc-opt':
        python + ' buildbot\\buildbot_standard.py opt 64 glibc',
    'xp-bare-newlib-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 newlib',
    'xp-bare-glibc-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 glibc',
    'win7-32-bare-newlib-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 newlib',
    'win7-32-bare-glibc-opt':
        python + ' buildbot\\buildbot_standard.py opt 32 glibc',
    'win7-64-bare-newlib-opt':
        python + ' buildbot\\buildbot_standard.py opt 64 newlib',
    'win7-64-bare-glibc-opt':
        python + ' buildbot\\buildbot_standard.py opt 64 glibc',
    'mac10.5-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'mac10.6-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'mac10.5-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'mac10.6-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'lucid-32-newlib-dbg':
        python + ' buildbot/buildbot_standard.py dbg 32 newlib',
    'lucid-32-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'lucid-32-glibc-dbg':
        python + ' buildbot/buildbot_standard.py dbg 32 glibc',
    'lucid-32-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'lucid-64-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'lucid-64-newlib-dbg':
        python + ' buildbot/buildbot_standard.py dbg 64 newlib',
    'lucid-64-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 64 newlib',
    'lucid-64-glibc-dbg':
        python + ' buildbot/buildbot_standard.py dbg 64 glibc',
    'lucid-64-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 64 glibc',
    'lucid-32-bare-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'lucid-32-bare-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'lucid-64-bare-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 64 newlib',
    'lucid-64-bare-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 64 glibc',

    # PNaCl.
    'lucid_64-newlib-arm_qemu-pnacl-dbg':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-dbg',
    'lucid_64-newlib-arm_qemu-pnacl-opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-opt',
    'jaunty_32-newlib-arm_hw-pnacl-dbg':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-hw-dbg',
    'jaunty_32-newlib-arm_hw-pnacl-opt':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-arm-hw-opt',
    'lucid_64-newlib-x86_32-pnacl':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8632',
    'lucid_64-newlib-x86_64-pnacl':
        'bash buildbot/buildbot_pnacl1.sh mode-buildbot-x8664',
    # PNaCl Spec
    'lucid_64-newlib-arm_qemu-pnacl-spec':
        'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-arm',
    'lucid_64-newlib-x86_32-pnacl-spec':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-x8632',
    'lucid_64-newlib-x86_64-pnacl-spec':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-pnacl-x8664',
    # NaCl Spec
    'lucid_64-newlib-x86_32-spec':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-nacl-x8632',
    'lucid_64-newlib-x86_64-spec':
      'bash buildbot/buildbot_pnacl2.sh mode-spec-nacl-x8664',

    # Valgrind bots.
    'lucid-64-newlib-dbg-valgrind': 'bash buildbot/buildbot_valgrind.sh newlib',
    'lucid-64-glibc-dbg-valgrind': 'bash buildbot/buildbot_valgrind.sh glibc',
    # Coverage.
    'mac10.6-newlib-coverage': 'bash buildbot/buildbot_coverage_mac.sh',
    'lucid-64-32-newlib-coverage':
        'bash buildbot/buildbot_coverage_linux.sh 32',
    'lucid-64-64-newlib-coverage':
        'bash buildbot/buildbot_coverage_linux.sh 64',
    'xp-newlib-coverage': 'buildbot\\buildbot_coverage_win.bat',
    # PPAPI Integration.
    'lucid64-m32-n32-opt-ppapi':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'lucid64-m64-n64-dbg-ppapi':
        python + ' buildbot/buildbot_standard.py dbg 64 newlib',

    ######################################################################
    # Trybots.
    ######################################################################
    'nacl-win32_newlib_opt':
        python + ' buildbot\\buildbot_standard.py opt 32 newlib',
    'nacl-win32_glibc_opt':
        python + ' buildbot\\buildbot_standard.py opt 32 glibc',
    'nacl-win64_newlib_dbg':
        python + ' buildbot\\buildbot_standard.py dbg 64 newlib',
    'nacl-win64_newlib_opt':
        python + ' buildbot\\buildbot_standard.py opt 64 newlib',
    'nacl-win64_glibc_opt':
        python + ' buildbot\\buildbot_standard.py opt 64 glibc',
    'nacl-mac10.5_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-mac10.5_glibc_opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'nacl-mac10.6_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-mac10.6_glibc_opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'nacl-lucid32_newlib_dbg':
        python + ' buildbot/buildbot_standard.py dbg 32 newlib',
    'nacl-lucid32_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-lucid32_glibc_opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'nacl-lucid64_newlib_dbg':
        python + ' buildbot/buildbot_standard.py dbg 64 newlib',
    'nacl-lucid64_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 64 newlib',
    'nacl-lucid64_glibc_opt':
        python + ' buildbot/buildbot_standard.py opt 64 glibc',
    'nacl-lucid64_newlib_dbg_valgrind':
        'bash buildbot/buildbot_valgrind.sh newlib',
    'nacl-lucid64_glibc_dbg_valgrind':
        'bash buildbot/buildbot_valgrind.sh glibc',
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
    'lucid64-toolchain_x86': 'bash buildbot/buildbot_toolchain.sh linux',
    # Pnacl toolchain builders.
    'linux-armtools-x86_32':
        'bash buildbot/buildbot_toolchain_arm_trusted.sh',
    'linux-pnacl-x86_32-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 32 newlib',
    'linux-pnacl-x86_64-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 64 newlib',
    'linux-pnacl-x86_64-glibc':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 64 glibc',
    'mac-pnacl-x86_32-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh mac 32 newlib',

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

    'nacl-toolchain-linux-pnacl-x86_32-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 32 newlib',
    'nacl-toolchain-linux-pnacl-x86_64-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 64 newlib',
    'nacl-toolchain-mac-pnacl-x86_32-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh mac 32 newlib',

}


IRT_ARCHIVE_BUILDERS = [
    'lucid-32-newlib-dbg',
    'lucid-64-newlib-dbg',
]
# Require a correspondence with the list above.
for builder in IRT_ARCHIVE_BUILDERS:
  assert builder in BOT_ASSIGNMENT, builder


def Main():
  builder = os.environ.get('BUILDBOT_BUILDERNAME')
  cmd = BOT_ASSIGNMENT.get(builder)
  if not cmd:
    sys.stderr.write('ERROR - unset/invalid builder name\n')
    sys.exit(1)

  env = os.environ.copy()
  env['ARCHIVE_IRT'] = builder in IRT_ARCHIVE_BUILDERS and '1' or '0'

  print "%s runs: %s\n" % (builder, cmd)
  retcode = subprocess.call(cmd, env=env, shell=True)
  sys.exit(retcode)


if __name__ == '__main__':
  Main()
