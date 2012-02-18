#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

python = sys.executable

# The system Python on Mac 10.5 is pretty old, use a newer version.
macpython27 = '/Library/Frameworks/Python.framework/Versions/2.7/bin/python'

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
        macpython27 + ' buildbot/buildbot_standard.py opt 32 newlib',
    'mac10.6-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'mac10.7-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'mac10.5-glibc-opt':
        macpython27 + ' buildbot/buildbot_standard.py opt 32 glibc',
    'mac10.6-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'mac10.7-glibc-opt':
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
    'lucid-64-validator-opt':
        'true',  # TODO(bradnelson): add a real build when archive is in place.
    'lucid-32-bare-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'lucid-32-bare-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'lucid-64-bare-newlib-opt':
        python + ' buildbot/buildbot_standard.py opt 64 newlib',
    'lucid-64-bare-glibc-opt':
        python + ' buildbot/buildbot_standard.py opt 64 glibc',

    # Clang.
    'lucid_64-newlib-dbg-clang':
      python + ' buildbot/buildbot_standard.py dbg 64 newlib --clang',
    'mac10.6-newlib-dbg-clang':
      python + ' buildbot/buildbot_standard.py dbg 32 newlib --clang',

    # ASan.
    'lucid_64-newlib-dbg-asan':
      python + ' buildbot/buildbot_standard.py opt 64 newlib --asan',
    'mac10.6-newlib-dbg-asan':
      python + ' buildbot/buildbot_standard.py opt 32 newlib --asan',

    # PNaCl.
    'lucid_64-newlib-arm_qemu-pnacl-dbg':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-dbg',
    'lucid_64-newlib-arm_qemu-pnacl-opt':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-opt',
    'jaunty_32-newlib-arm_hw-pnacl-dbg':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-dbg',
    'jaunty_32-newlib-arm_hw-pnacl-opt':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-opt',
    'oneiric_32-newlib-arm_hw-pnacl-panda-opt':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-opt',
    'lucid_64-newlib-x86_32-pnacl':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-x8632',
    'lucid_64-newlib-x86_64-pnacl':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-x8664',
    # PNaCl Spec
    'lucid_64-newlib-arm_qemu-pnacl-spec':
        'bash buildbot/buildbot_spec2k.sh pnacl-arm',
    'lucid_64-newlib-x86_32-pnacl-spec':
      'bash buildbot/buildbot_spec2k.sh pnacl-x8632',
    'lucid_64-newlib-x86_64-pnacl-spec':
      'bash buildbot/buildbot_spec2k.sh pnacl-x8664',
    # NaCl Spec
    'lucid_64-newlib-x86_32-spec':
      'bash buildbot/buildbot_spec2k.sh nacl-x8632',
    'lucid_64-newlib-x86_64-spec':
      'bash buildbot/buildbot_spec2k.sh nacl-x8664',

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
        macpython27 + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-mac10.5_glibc_opt':
        macpython27 + ' buildbot/buildbot_standard.py opt 32 glibc',
    'nacl-mac10.6_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-mac10.6_glibc_opt':
        python + ' buildbot/buildbot_standard.py opt 32 glibc',
    'nacl-mac10.7_newlib_opt':
        python + ' buildbot/buildbot_standard.py opt 32 newlib',
    'nacl-mac10.7_glibc_opt':
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
    'nacl-lucid64_validator_opt':
        'true',  # TODO(bradnelson): add a real build when archive is in place.
    'nacl-lucid64_newlib_dbg_valgrind':
        'bash buildbot/buildbot_valgrind.sh newlib',
    'nacl-lucid64_glibc_dbg_valgrind':
        'bash buildbot/buildbot_valgrind.sh glibc',
    # Clang trybots.
    'nacl-lucid_64-newlib-dbg-clang':
        python + ' buildbot/buildbot_standard.py dbg 64 newlib --clang',
    'nacl-mac10.6-newlib-dbg-clang':
        python + ' buildbot/buildbot_standard.py dbg 32 newlib --clang',
    # Pnacl scons trybots
    'nacl-arm_opt':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-try',
    'nacl-arm_hw_opt':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-try',
    # Pnacl scons trybots
    'nacl-arm_opt_panda':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-try',
    'nacl-arm_hw_opt_panda':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-try',
    # Pnacl perf scons trybots
    # TODO(sehr): hook these up to something useful.
    'nacl-arm_perf_panda':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-try',
    'nacl-arm_hw_perf_panda':
        'bash buildbot/buildbot_pnacl.sh mode-buildbot-arm-hw-try',
    'nacl-lucid_64-newlib-arm_qemu-pnacl':
        'bash buildbot/buildbot_pnacl.sh mode-trybot-arm',
    'nacl-lucid_64-newlib-x86_32-pnacl':
        'bash buildbot/buildbot_pnacl.sh mode-trybot-x8632',
    'nacl-lucid_64-newlib-x86_64-pnacl':
        'bash buildbot/buildbot_pnacl.sh mode-trybot-x8664',
    # Pnacl spec2k trybots
    'nacl-lucid_64-newlib-arm_qemu-pnacl-spec':
        'bash buildbot/buildbot_spec2k.sh pnacl-trybot-arm',
    'nacl-lucid_64-newlib-x86_32-pnacl-spec':
        'bash buildbot/buildbot_spec2k.sh pnacl-trybot-x8632',
    'nacl-lucid_64-newlib-x86_64-pnacl-spec':
        'bash buildbot/buildbot_spec2k.sh pnacl-trybot-x8664',
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
    'linux-pnacl-x86_32':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 32',
    'linux-pnacl-x86_64':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 64',
    'mac-pnacl-x86_32':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh mac 32',
    'linux-pnacl-x86_64-tests':
        'bash buildbot/buildbot_pnacl_toolchain_tests.sh',
    # TODO(pdox): Rename this bot to x86_64
    'win-pnacl-x86_32':
        'buildbot\\buildbot_toolchain_arm_untrusted.bat win 64',

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
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 32',
    'nacl-toolchain-linux-pnacl-x86_64-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh linux 64',
    'nacl-toolchain-mac-pnacl-x86_32-newlib':
        'bash buildbot/buildbot_toolchain_arm_untrusted.sh mac 32',

    # PNaCl LLVM Merging bots
    'llvm':
        'bash buildbot/buildbot_pnacl_merge.sh merge-bot',
    'llvm-scons':
        'bash buildbot/buildbot_pnacl_merge.sh scons-bot',
    'llvm-spec2k-x86':
        'bash buildbot/buildbot_pnacl_merge.sh spec2k-x86-bot',
    'llvm-spec2k-arm':
        'bash buildbot/buildbot_pnacl_merge.sh spec2k-arm-bot',

}


IRT_ARCHIVE_BUILDERS = [
    'lucid-32-newlib-dbg',
    'lucid-64-newlib-dbg',
]
def CheckBuilderMap():
  # Require a correspondence with the list above.
  for builder in IRT_ARCHIVE_BUILDERS:
    assert builder in BOT_ASSIGNMENT, builder
CheckBuilderMap()


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
