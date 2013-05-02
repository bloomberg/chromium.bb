#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point for both build and try bots.

This script is invoked from XXX, usually without arguments
to package an SDK. It automatically determines whether
this SDK is for mac, win, linux.

The script inspects the following environment variables:

BUILDBOT_BUILDERNAME to determine whether the script is run locally
and whether it should upload an SDK to file storage (GSTORE)
"""

# pylint: disable=W0621

# std python includes
import copy
import datetime
import optparse
import os
import re
import sys

if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)

# local includes
import buildbot_common
import build_projects
import build_updater
import build_version
import generate_make
import generate_notice
import manifest_util
import parse_dsc
import test_sdk

from build_paths import SDK_SRC_DIR, SRC_DIR, NACL_DIR, OUT_DIR
from build_paths import PPAPI_DIR, NACLPORTS_DIR, GSTORE

# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))

import getos
import oshelpers

CYGTAR = os.path.join(NACL_DIR, 'build', 'cygtar.py')

NACLPORTS_URL = 'https://naclports.googlecode.com/svn/trunk/src'
NACLPORTS_REV = 712

options = None


def GetGlibcToolchain(platform, arch):
  tcdir = os.path.join(NACL_DIR, 'toolchain', '.tars')
  tcname = 'toolchain_%s_%s.tar.bz2' % (platform, arch)
  return os.path.join(tcdir, tcname)


def GetNewlibToolchain(platform, arch):
  tcdir = os.path.join(NACL_DIR, 'toolchain', '.tars')
  tcname = 'naclsdk_%s_%s.tgz' % (platform, arch)
  return os.path.join(tcdir, tcname)


def GetPNaClToolchain(os_platform, arch):
  tcdir = os.path.join(NACL_DIR, 'toolchain', '.tars')
  tcname = 'naclsdk_pnacl_%s_%s.tgz' % (os_platform, arch)
  return os.path.join(tcdir, tcname)


def GetScons():
  if sys.platform in ['cygwin', 'win32']:
    return 'scons.bat'
  return './scons'


def GetArchName(arch, xarch=None):
  if xarch:
    return arch + '-' + str(xarch)
  return arch


def GetToolchainNaClInclude(tcname, tcpath, arch):
  if arch == 'x86':
    if tcname == 'pnacl':
      return os.path.join(tcpath, 'newlib', 'sdk', 'include')
    return os.path.join(tcpath, 'x86_64-nacl', 'include')
  elif arch == 'arm':
    return os.path.join(tcpath, 'arm-nacl', 'include')
  else:
    buildbot_common.ErrorExit('Unknown architecture: %s' % arch)


def GetToolchainNaClLib(tcname, tcpath, arch, xarch):
  if arch == 'x86':
    if tcname == 'pnacl':
      return os.path.join(tcpath, 'newlib', 'sdk', 'lib')
    if str(xarch) == '32':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
    if str(xarch) == '64':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib')
    if str(xarch) == 'arm':
      return os.path.join(tcpath, 'arm-nacl', 'lib')
  buildbot_common.ErrorExit('Unknown architecture: %s' % arch)


def GetPNaClNativeLib(tcpath, arch):
  if arch not in ['arm', 'x86-32', 'x86-64']:
    buildbot_common.ErrorExit('Unknown architecture %s.' % arch)
  return os.path.join(tcpath, 'lib-' + arch)


def GetBuildArgs(tcname, tcpath, outdir, arch, xarch=None):
  """Return list of scons build arguments to generate user libraries."""
  scons = GetScons()
  mode = '--mode=opt-host,nacl'
  arch_name = GetArchName(arch, xarch)
  plat = 'platform=' + arch_name
  binarg = 'bindir=' + os.path.join(outdir, 'tools')
  lib = 'libdir=' + GetToolchainNaClLib(tcname, tcpath, arch, xarch)
  args = [scons, mode, plat, binarg, lib, '-j10',
          'install_bin', 'install_lib']
  if tcname == 'glibc':
    args.append('--nacl_glibc')

  if tcname == 'pnacl':
    args.append('bitcode=1')

  print "Building %s (%s): %s" % (tcname, arch, ' '.join(args))
  return args


def BuildStepDownloadToolchains():
  buildbot_common.BuildStep('Running download_toolchains.py')
  download_script = os.path.join('build', 'download_toolchains.py')
  buildbot_common.Run([sys.executable, download_script,
                      '--no-arm-trusted', '--arm-untrusted', '--keep'],
                      cwd=NACL_DIR)


def BuildStepCleanPepperDirs(pepperdir, pepperdir_old):
  buildbot_common.BuildStep('Clean Pepper Dirs')
  buildbot_common.RemoveDir(pepperdir_old)
  buildbot_common.RemoveDir(pepperdir)
  buildbot_common.MakeDir(pepperdir)


def BuildStepMakePepperDirs(pepperdir, subdirs):
  for subdir in subdirs:
    buildbot_common.MakeDir(os.path.join(pepperdir, subdir))


def BuildStepCopyTextFiles(pepperdir, pepper_ver, revision):
  buildbot_common.BuildStep('Add Text Files')
  files = ['AUTHORS', 'COPYING', 'LICENSE']
  files = [os.path.join(SDK_SRC_DIR, filename) for filename in files]
  oshelpers.Copy(['-v'] + files + [pepperdir])

  # Replace a few placeholders in README
  readme_text = open(os.path.join(SDK_SRC_DIR, 'README')).read()
  readme_text = readme_text.replace('${VERSION}', pepper_ver)
  readme_text = readme_text.replace('${REVISION}', revision)

  # Year/Month/Day Hour:Minute:Second
  time_format = '%Y/%m/%d %H:%M:%S'
  readme_text = readme_text.replace('${DATE}',
      datetime.datetime.now().strftime(time_format))

  open(os.path.join(pepperdir, 'README'), 'w').write(readme_text)


def BuildStepUntarToolchains(pepperdir, platform, arch, toolchains):
  buildbot_common.BuildStep('Untar Toolchains')
  tcname = platform + '_' + arch
  tmpdir = os.path.join(OUT_DIR, 'tc_temp')
  buildbot_common.RemoveDir(tmpdir)
  buildbot_common.MakeDir(tmpdir)

  if 'newlib' in toolchains:
    # Untar the newlib toolchains
    tarfile = GetNewlibToolchain(platform, arch)
    buildbot_common.Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile],
                        cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'sdk', 'nacl-sdk')
    newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
    buildbot_common.Move(srcdir, newlibdir)

  if 'arm' in toolchains:
    # Copy the existing arm toolchain from native_client tree
    arm_toolchain = os.path.join(NACL_DIR, 'toolchain',
                                 platform + '_arm_newlib')
    arm_toolchain_sdk = os.path.join(pepperdir, 'toolchain',
                                     os.path.basename(arm_toolchain))
    buildbot_common.CopyDir(arm_toolchain, arm_toolchain_sdk)

  if 'glibc' in toolchains:
    # Untar the glibc toolchains
    tarfile = GetGlibcToolchain(platform, arch)
    buildbot_common.Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile],
                        cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'toolchain', tcname)
    glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
    buildbot_common.Move(srcdir, glibcdir)

  # Untar the pnacl toolchains
  if 'pnacl' in toolchains:
    tmpdir = os.path.join(tmpdir, 'pnacl')
    buildbot_common.RemoveDir(tmpdir)
    buildbot_common.MakeDir(tmpdir)
    tarfile = GetPNaClToolchain(platform, arch)
    buildbot_common.Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile],
                        cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    pnacldir = os.path.join(pepperdir, 'toolchain', tcname + '_pnacl')
    buildbot_common.Move(tmpdir, pnacldir)

  if options.gyp and sys.platform not in ['cygwin', 'win32']:
    # If the gyp options is specified we install a toolchain
    # wrapper so that gyp can switch toolchains via a commandline
    # option.
    bindir = os.path.join(pepperdir, 'toolchain', tcname, 'bin')
    wrapper = os.path.join(SDK_SRC_DIR, 'tools', 'compiler-wrapper.py')
    buildbot_common.MakeDir(bindir)
    buildbot_common.CopyFile(wrapper, bindir)

    # Module 'os' has no 'symlink' member (on Windows).
    # pylint: disable=E1101

    os.symlink('compiler-wrapper.py', os.path.join(bindir, 'i686-nacl-g++'))
    os.symlink('compiler-wrapper.py', os.path.join(bindir, 'i686-nacl-gcc'))
    os.symlink('compiler-wrapper.py', os.path.join(bindir, 'i686-nacl-ar'))


HEADER_MAP = {
  'newlib': {
      'pthread.h': 'src/untrusted/pthread/pthread.h',
      'semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'nacl/nacl_dyncode.h': 'src/untrusted/nacl/nacl_dyncode.h',
      'nacl/nacl_exception.h': 'src/include/nacl/nacl_exception.h',
      'nacl/nacl_startup.h': 'src/untrusted/nacl/nacl_startup.h',
      'nacl/nacl_thread.h': 'src/untrusted/nacl/nacl_thread.h',
      'pnacl.h': 'src/untrusted/nacl/pnacl.h',
      'irt.h': 'src/untrusted/irt/irt.h',
      'irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
  'glibc': {
      'nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'nacl/nacl_dyncode.h': 'src/untrusted/nacl/nacl_dyncode.h',
      'nacl/nacl_exception.h': 'src/include/nacl/nacl_exception.h',
      'nacl/nacl_startup.h': 'src/untrusted/nacl/nacl_startup.h',
      'nacl/nacl_thread.h': 'src/untrusted/nacl/nacl_thread.h',
      'pnacl.h': 'src/untrusted/nacl/pnacl.h',
      'irt.h': 'src/untrusted/irt/irt.h',
      'irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
  'host': {
  },
}


def InstallCommonHeaders(inc_path):
  # Clean out per toolchain ppapi directory
  ppapi = os.path.join(inc_path, 'ppapi')
  buildbot_common.RemoveDir(ppapi)

  # Copy in c, c/dev and c/extensions/dev headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'c', 'dev'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'c', '*.h'),
          os.path.join(ppapi, 'c'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'c', 'dev', '*.h'),
          os.path.join(ppapi, 'c', 'dev'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'c', 'extensions', 'dev'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'c', 'extensions', 'dev', '*.h'),
          os.path.join(ppapi, 'c', 'extensions', 'dev'))

  # Remove private and trusted interfaces
  buildbot_common.RemoveDir(os.path.join(ppapi, 'c', 'private'))
  buildbot_common.RemoveDir(os.path.join(ppapi, 'c', 'trusted'))

  # Copy in the C++ headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'cpp', 'dev'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'cpp', '*.h'),
          os.path.join(ppapi, 'cpp'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'cpp', 'dev', '*.h'),
          os.path.join(ppapi, 'cpp', 'dev'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'cpp', 'extensions', 'dev'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'cpp', 'extensions', '*.h'),
          os.path.join(ppapi, 'cpp', 'extensions'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'cpp', 'extensions', 'dev', '*.h'),
          os.path.join(ppapi, 'cpp', 'extensions', 'dev'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'graphics'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'threading'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'websocket'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'utility', '*.h'),
          os.path.join(ppapi, 'utility'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'utility', 'graphics', '*.h'),
          os.path.join(ppapi, 'utility', 'graphics'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'utility', 'threading', '*.h'),
          os.path.join(ppapi, 'utility', 'threading'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'utility', 'websocket', '*.h'),
          os.path.join(ppapi, 'utility', 'websocket'))

  # Copy in the gles2 headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'gles2'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'lib', 'gl', 'gles2', '*.h'),
          os.path.join(ppapi, 'gles2'))

  # Copy the EGL headers
  buildbot_common.MakeDir(os.path.join(inc_path, 'EGL'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'EGL', '*.h'),
          os.path.join(inc_path, 'EGL'))

  # Copy the GLES2 headers
  buildbot_common.MakeDir(os.path.join(inc_path, 'GLES2'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'GLES2', '*.h'),
          os.path.join(inc_path, 'GLES2'))

  # Copy the KHR headers
  buildbot_common.MakeDir(os.path.join(inc_path, 'KHR'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'KHR', '*.h'),
          os.path.join(inc_path, 'KHR'))

  # Copy the lib files
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'lib'),
          os.path.join(inc_path, 'ppapi'))


def InstallNaClHeaders(tc_dst_inc, tc_name):
  """Copies NaCl headers to expected locations in the toolchain."""
  if tc_name == 'arm':
    # arm toolchain header should be the same as the x86 newlib
    # ones
    tc_name = 'newlib'
  tc_map = HEADER_MAP[tc_name]

  for filename in tc_map:
    src = os.path.join(NACL_DIR, tc_map[filename])
    dst = os.path.join(tc_dst_inc, filename)
    buildbot_common.MakeDir(os.path.dirname(dst))
    buildbot_common.CopyFile(src, dst)


def MakeNinjaRelPath(path):
  return os.path.join(os.path.relpath(OUT_DIR, SRC_DIR), path)


def GypNinjaInstall(pepperdir, platform, toolchains):
  build_dir = 'gypbuild'
  ninja_out_dir = os.path.join(OUT_DIR, build_dir, 'Release')
  # src_file, dst_file, is_host_exe?
  tools_files = [
    ('sel_ldr', 'sel_ldr_x86_32', True),
    ('ncval_x86_32', 'ncval_x86_32', True),
    ('ncval_arm', 'ncval_arm', True),
    ('irt_core_newlib_x32.nexe', 'irt_core_newlib_x32.nexe', False),
    ('irt_core_newlib_x64.nexe', 'irt_core_newlib_x64.nexe', False),
  ]
  if platform != 'mac':
    # Mac doesn't build 64-bit binaries.
    tools_files.append(('sel_ldr64', 'sel_ldr_x86_64', True))
    tools_files.append(('ncval_x86_64', 'ncval_x86_64', True))

  if platform == 'linux':
    tools_files.append(('nacl_helper_bootstrap',
                        'nacl_helper_bootstrap_x86_32', True))
    tools_files.append(('nacl_helper_bootstrap64',
                        'nacl_helper_bootstrap_x86_64', True))

  buildbot_common.MakeDir(os.path.join(pepperdir, 'tools'))
  for src, dst, host_exe in tools_files:
    if platform == 'win' and host_exe:
      src += '.exe'
      dst += '.exe'

    buildbot_common.CopyFile(
        os.path.join(ninja_out_dir, src),
        os.path.join(pepperdir, 'tools', dst))

  for tc in set(toolchains) & set(['newlib', 'glibc']):
    for archname in ('arm', '32', '64'):
      if tc == 'glibc' and archname == 'arm':
        continue
      tc_dir = 'tc_' + tc
      lib_dir = 'lib' + archname
      if archname == 'arm':
        build_dir = 'gypbuild-arm'
        tcdir = '%s_arm_%s' % (platform, tc)
      else:
        build_dir = 'gypbuild'
        tcdir = '%s_x86_%s' % (platform, tc)

      ninja_out_dir = os.path.join(OUT_DIR, build_dir, 'Release')
      src_dir = os.path.join(ninja_out_dir, 'gen', tc_dir, lib_dir)
      tcpath = os.path.join(pepperdir, 'toolchain', tcdir)
      dst_dir = GetToolchainNaClLib(tc, tcpath, 'x86', archname)

      buildbot_common.MakeDir(dst_dir)
      buildbot_common.CopyDir(os.path.join(src_dir, '*.a'), dst_dir)
      if tc == 'newlib':
        buildbot_common.CopyDir(os.path.join(src_dir, '*.o'), dst_dir)

      if tc == 'glibc':
        buildbot_common.CopyDir(os.path.join(src_dir, '*.so'), dst_dir)

      ninja_tcpath = os.path.join(ninja_out_dir, 'gen', 'sdk', 'toolchain',
                      tcdir)
      lib_dir = GetToolchainNaClLib(tc, ninja_tcpath, 'x86', archname)
      buildbot_common.CopyFile(os.path.join(lib_dir, 'crt1.o'), dst_dir)


def GypNinjaBuild_NaCl(platform, rel_out_dir):
  gyp_py = os.path.join(NACL_DIR, 'build', 'gyp_nacl')
  nacl_core_sdk_gyp = os.path.join(NACL_DIR, 'build', 'nacl_core_sdk.gyp')
  all_gyp = os.path.join(NACL_DIR, 'build', 'all.gyp')

  out_dir = MakeNinjaRelPath(rel_out_dir)
  out_dir_arm = MakeNinjaRelPath(rel_out_dir + '-arm')
  GypNinjaBuild('ia32', gyp_py, nacl_core_sdk_gyp, 'nacl_core_sdk', out_dir)
  GypNinjaBuild('arm', gyp_py, nacl_core_sdk_gyp, 'nacl_core_sdk', out_dir_arm)
  GypNinjaBuild('ia32', gyp_py, all_gyp, 'ncval_x86_32', out_dir)
  GypNinjaBuild(None, gyp_py, all_gyp, 'ncval_arm', out_dir)

  if platform == 'win':
    NinjaBuild('sel_ldr64', out_dir)
    NinjaBuild('ncval_x86_64', out_dir)
  elif platform == 'linux':
    out_dir_64 = MakeNinjaRelPath(rel_out_dir + '-64')
    GypNinjaBuild('x64', gyp_py, nacl_core_sdk_gyp, 'sel_ldr', out_dir_64)
    GypNinjaBuild('x64', gyp_py, all_gyp, 'ncval_x86_64', out_dir_64)

    # We only need sel_ldr and ncval_x86_64 from the 64-bit out directory.
    # sel_ldr needs to be renamed, so we'll call it sel_ldr64.
    files_to_copy = [
      ('sel_ldr', 'sel_ldr64'),
      ('ncval_x86_64', 'ncval_x86_64'),
    ]
    files_to_copy.append(('nacl_helper_bootstrap', 'nacl_helper_bootstrap64'))

    for src, dst in files_to_copy:
      buildbot_common.CopyFile(
          os.path.join(SRC_DIR, out_dir_64, 'Release', src),
          os.path.join(SRC_DIR, out_dir, 'Release', dst))


def GypNinjaBuild_Chrome(arch, rel_out_dir):
  gyp_py = os.path.join(SRC_DIR, 'build', 'gyp_chromium')

  out_dir = MakeNinjaRelPath(rel_out_dir)
  gyp_file = os.path.join(SRC_DIR, 'ppapi', 'ppapi_untrusted.gyp')
  targets = ['ppapi_cpp_lib', 'ppapi_gles2_lib']
  GypNinjaBuild(arch, gyp_py, gyp_file, targets, out_dir)

  gyp_file = os.path.join(SRC_DIR, 'ppapi', 'native_client',
                          'native_client.gyp')
  GypNinjaBuild(arch, gyp_py, gyp_file, 'ppapi_lib', out_dir)


def GypNinjaBuild_Pnacl(rel_out_dir, target_arch):
  # TODO(binji): This will build the pnacl_irt_shim twice; once as part of the
  # Chromium build, and once here. When we move more of the SDK build process
  # to gyp, we can remove this.
  gyp_py = os.path.join(SRC_DIR, 'build', 'gyp_chromium')

  out_dir = MakeNinjaRelPath(rel_out_dir)
  gyp_file = os.path.join(SRC_DIR, 'ppapi', 'native_client', 'src',
                          'untrusted', 'pnacl_irt_shim', 'pnacl_irt_shim.gyp')
  targets = ['pnacl_irt_shim']
  GypNinjaBuild(target_arch, gyp_py, gyp_file, targets, out_dir, False)


def GypNinjaBuild(arch, gyp_py_script, gyp_file, targets,
                  out_dir, force_arm_gcc=True):
  gyp_env = copy.copy(os.environ)
  gyp_env['GYP_GENERATORS'] = 'ninja'
  gyp_defines = []
  if options.mac_sdk:
    gyp_defines.append('mac_sdk=%s' % options.mac_sdk)
  if arch:
    gyp_defines.append('target_arch=%s' % arch)
    if arch == 'arm':
      gyp_defines += ['armv7=1', 'arm_thumb=0', 'arm_neon=1']
      if force_arm_gcc:
        gyp_defines += ['nacl_enable_arm_gcc=1']

  gyp_env['GYP_DEFINES'] = ' '.join(gyp_defines)
  for key in ['GYP_GENERATORS', 'GYP_DEFINES']:
    value = gyp_env[key]
    print '%s="%s"' % (key, value)
  gyp_generator_flags = ['-G', 'output_dir=%s' % (out_dir,)]
  gyp_depth = '--depth=.'
  buildbot_common.Run(
      [sys.executable, gyp_py_script, gyp_file, gyp_depth] + \
          gyp_generator_flags,
      cwd=SRC_DIR,
      env=gyp_env)
  NinjaBuild(targets, out_dir)


def NinjaBuild(targets, out_dir):
  if type(targets) is not list:
    targets = [targets]
  out_config_dir = os.path.join(out_dir, 'Release')
  buildbot_common.Run(['ninja', '-C', out_config_dir] + targets, cwd=SRC_DIR)


def BuildStepBuildToolchains(pepperdir, platform, toolchains):
  buildbot_common.BuildStep('SDK Items')

  GypNinjaBuild_NaCl(platform, 'gypbuild')

  tcname = platform + '_x86'
  newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
  glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
  pnacldir = os.path.join(pepperdir, 'toolchain', tcname + '_pnacl')

  # Run scons TC build steps
  if set(toolchains) & set(['glibc', 'newlib']):
    GypNinjaBuild_Chrome('ia32', 'gypbuild')

  if 'arm' in toolchains:
    GypNinjaBuild_Chrome('arm', 'gypbuild-arm')

  GypNinjaInstall(pepperdir, platform, toolchains)

  if 'newlib' in toolchains:
    InstallNaClHeaders(GetToolchainNaClInclude('newlib', newlibdir, 'x86'),
                       'newlib')

  if 'glibc' in toolchains:
    InstallNaClHeaders(GetToolchainNaClInclude('glibc', glibcdir, 'x86'),
                       'glibc')

  if 'arm' in toolchains:
    tcname = platform + '_arm_newlib'
    armdir = os.path.join(pepperdir, 'toolchain', tcname)
    InstallNaClHeaders(GetToolchainNaClInclude('newlib', armdir, 'arm'),
                       'arm')

  if 'pnacl' in toolchains:
    shell = platform == 'win'
    buildbot_common.Run(
        GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '32'),
        cwd=NACL_DIR, shell=shell)
    buildbot_common.Run(
        GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '64'),
        cwd=NACL_DIR, shell=shell)

    for arch in ('ia32', 'arm'):
      # Fill in the latest native pnacl shim library from the chrome build.
      build_dir = 'gypbuild-pnacl-' + arch
      GypNinjaBuild_Pnacl(build_dir, arch)
      pnacl_libdir_map = {'ia32': 'x86-64', 'arm': 'arm'}
      release_build_dir = os.path.join(OUT_DIR, build_dir, 'Release',
                                       'gen', 'tc_pnacl_translate',
                                       'lib-' + pnacl_libdir_map[arch])

      buildbot_common.CopyFile(
          os.path.join(release_build_dir, 'libpnacl_irt_shim.a'),
          GetPNaClNativeLib(pnacldir, pnacl_libdir_map[arch]))

    InstallNaClHeaders(GetToolchainNaClInclude('pnacl', pnacldir, 'x86'),
                       'newlib')




def MakeDirectoryOrClobber(pepperdir, dirname, clobber):
  dirpath = os.path.join(pepperdir, dirname)
  if clobber:
    buildbot_common.RemoveDir(dirpath)
  buildbot_common.MakeDir(dirpath)

  return dirpath


def BuildStepUpdateHelpers(pepperdir, platform, clobber):
  buildbot_common.BuildStep('Update project helpers')
  build_projects.UpdateHelpers(pepperdir, platform, clobber=clobber)


def BuildStepUpdateUserProjects(pepperdir, platform, toolchains,
                                build_experimental, clobber):
  buildbot_common.BuildStep('Update examples and libraries')

  filters = {}
  if not build_experimental:
    filters['EXPERIMENTAL'] = False
  if toolchains:
    filters['TOOLS'] = toolchains

  # Update examples and libraries
  filters['DEST'] = ['examples', 'src']

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  build_projects.UpdateProjects(pepperdir, platform, tree, clobber=clobber,
                                toolchains=toolchains)


def BuildStepMakeAll(pepperdir, platform, directory, step_name,
                     clean=False, deps=True, config='Debug'):
  buildbot_common.BuildStep(step_name)
  make_dir = os.path.join(pepperdir, directory)

  print "\n\nMake: " + make_dir
  if platform == 'win':
    make = os.path.join(make_dir, 'make.bat')
  else:
    make = 'make'

  extra_args = ['CONFIG='+config]
  if not deps:
    extra_args += ['IGNORE_DEPS=1']

  buildbot_common.Run([make, '-j8', 'all_versions'] + extra_args,
                      cwd=make_dir)
  if clean:
    # Clean to remove temporary files but keep the built libraries.
    buildbot_common.Run([make, '-j8', 'clean'] + extra_args, cwd=make_dir)


def BuildStepBuildLibraries(pepperdir, platform, directory, clean=True):
  BuildStepMakeAll(pepperdir, platform, directory, 'Build Libraries Debug',
      clean=clean, config='Debug')
  BuildStepMakeAll(pepperdir, platform, directory, 'Build Libraries Release',
      clean=clean, config='Release')


def GenerateNotice(fileroot, output_filename='NOTICE', extra_files=None):
  # Look for LICENSE files
  license_filenames_re = re.compile('LICENSE|COPYING|COPYRIGHT')

  license_files = []
  for root, _, files in os.walk(fileroot):
    for filename in files:
      if license_filenames_re.match(filename):
        path = os.path.join(root, filename)
        license_files.append(path)

  if extra_files:
    license_files += [os.path.join(fileroot, f) for f in extra_files]
  print '\n'.join(license_files)

  if not os.path.isabs(output_filename):
    output_filename = os.path.join(fileroot, output_filename)
  generate_notice.Generate(output_filename, fileroot, license_files)


def BuildStepTarBundle(pepper_ver, tarfile):
  buildbot_common.BuildStep('Tar Pepper Bundle')
  buildbot_common.MakeDir(os.path.dirname(tarfile))
  buildbot_common.Run([sys.executable, CYGTAR, '-C', OUT_DIR, '-cjf', tarfile,
       'pepper_' + pepper_ver], cwd=NACL_DIR)


def BuildStepRunUnittests():
  buildbot_common.BuildStep('Run unittests')
  test_all_py = os.path.join(SDK_SRC_DIR, 'test_all.py')

  # Our tests shouldn't be using the proxy; they should all be connecting to
  # localhost. Some slaves can't route HTTP traffic through the proxy to
  # localhost (we get 504 gateway errors), so we clear it here.
  env = dict(os.environ)
  if 'http_proxy' in env:
    del env['http_proxy']
  buildbot_common.Run([sys.executable, test_all_py], env=env)


def BuildStepTestSDK():
  args = []
  if options.build_experimental:
    args.append('--experimental')
  test_sdk.main(args)


def GetManifestBundle(pepper_ver, revision, tarfile, archive_url):
  with open(tarfile, 'rb') as tarfile_stream:
    archive_sha1, archive_size = manifest_util.DownloadAndComputeHash(
        tarfile_stream)

  archive = manifest_util.Archive(manifest_util.GetHostOS())
  archive.url = archive_url
  archive.size = archive_size
  archive.checksum = archive_sha1

  bundle = manifest_util.Bundle('pepper_' + pepper_ver)
  bundle.revision = int(revision)
  bundle.repath = 'pepper_' + pepper_ver
  bundle.version = int(pepper_ver)
  bundle.description = 'Chrome %s bundle, revision %s' % (pepper_ver, revision)
  bundle.stability = 'dev'
  bundle.recommended = 'no'
  bundle.archives = [archive]
  return bundle


def BuildStepArchiveBundle(name, pepper_ver, revision, tarfile):
  buildbot_common.BuildStep('Archive %s' % name)
  bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s' % (
      build_version.ChromeVersion(),)
  tarname = os.path.basename(tarfile)
  tarfile_dir = os.path.dirname(tarfile)
  buildbot_common.Archive(tarname, bucket_path, tarfile_dir)

  # generate "manifest snippet" for this archive.
  archive_url = GSTORE + 'nacl_sdk/%s/%s' % (
      build_version.ChromeVersion(), tarname)
  bundle = GetManifestBundle(pepper_ver, revision, tarfile, archive_url)

  manifest_snippet_file = os.path.join(OUT_DIR, tarname + '.json')
  with open(manifest_snippet_file, 'wb') as manifest_snippet_stream:
    manifest_snippet_stream.write(bundle.GetDataAsString())

  buildbot_common.Archive(tarname + '.json', bucket_path, OUT_DIR,
                          step_link=False)


def BuildStepArchiveSDKTools():
  # Only push up sdk_tools.tgz and nacl_sdk.zip on the linux buildbot.
  builder_name = os.getenv('BUILDBOT_BUILDERNAME', '')
  if builder_name == 'linux-sdk-multi':
    buildbot_common.BuildStep('Build SDK Tools')
    build_updater.BuildUpdater(OUT_DIR)

    buildbot_common.BuildStep('Archive SDK Tools')
    bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s' % (
        build_version.ChromeVersion(),)
    buildbot_common.Archive('sdk_tools.tgz', bucket_path, OUT_DIR,
                            step_link=False)
    buildbot_common.Archive('nacl_sdk.zip', bucket_path, OUT_DIR,
                            step_link=False)


def BuildStepSyncNaClPorts():
  """Pull the pinned revision of naclports from SVN."""
  buildbot_common.BuildStep('Sync naclports')
  if not os.path.exists(NACLPORTS_DIR):
    # checkout new copy of naclports
    cmd = ['svn', 'checkout', '-q', '-r', str(NACLPORTS_REV), NACLPORTS_URL,
           'naclports']
    buildbot_common.Run(cmd, cwd=os.path.dirname(NACLPORTS_DIR))
  else:
    # sync existing copy to pinned revision.
    cmd = ['svn', 'update', '-r', str(NACLPORTS_REV)]
    buildbot_common.Run(cmd, cwd=NACLPORTS_DIR)


def BuildStepBuildNaClPorts(pepper_ver, pepperdir):
  """Build selected naclports in all configurations."""
  # TODO(sbc): currently naclports doesn't know anything about
  # Debug builds so the Debug subfolders are all empty.
  bundle_dir = os.path.join(NACLPORTS_DIR, 'out', 'sdk_bundle')

  env = dict(os.environ)
  env['NACL_SDK_ROOT'] = pepperdir
  env['NACLPORTS_NO_ANNOTATE'] = "1"

  build_script = 'build_tools/bots/linux/nacl-linux-sdk-bundle.sh'
  buildbot_common.BuildStep('Build naclports')
  buildbot_common.Run([build_script], env=env, cwd=NACLPORTS_DIR)

  out_dir = os.path.join(bundle_dir, 'pepper_XX')
  out_dir_final = os.path.join(bundle_dir, 'pepper_%s' % pepper_ver)
  buildbot_common.RemoveDir(out_dir_final)
  buildbot_common.Move(out_dir, out_dir_final)

  # Some naclports do not include a standalone LICENSE/COPYING file
  # so we explicitly list those here for inclusion.
  extra_licenses = ('tinyxml/readme.txt',
                    'jpeg-8d/README',
                    'zlib-1.2.3/README')
  src_root = os.path.join(NACLPORTS_DIR, 'out', 'repository-i686')
  output_license = os.path.join(out_dir_final, 'ports', 'LICENSE')
  GenerateNotice(src_root , output_license, extra_licenses)
  readme = os.path.join(out_dir_final, 'ports', 'README')
  oshelpers.Copy(['-v', os.path.join(SDK_SRC_DIR, 'README.naclports'), readme])


def BuildStepTarNaClPorts(pepper_ver, tarfile):
  """Create tar archive containing headers and libs from naclports build."""
  buildbot_common.BuildStep('Tar naclports Bundle')
  buildbot_common.MakeDir(os.path.dirname(tarfile))
  pepper_dir = 'pepper_%s' % pepper_ver
  archive_dirs = [os.path.join(pepper_dir, 'ports')]

  ports_out = os.path.join(NACLPORTS_DIR, 'out', 'sdk_bundle')
  cmd = [sys.executable, CYGTAR, '-C', ports_out, '-cjf', tarfile]
  cmd += archive_dirs
  buildbot_common.Run(cmd, cwd=NACL_DIR)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--run-tests',
      help='Run tests. This includes building examples.', action='store_true')
  parser.add_option('--skip-tar', help='Skip generating a tarball.',
      action='store_true')
  parser.add_option('--archive', help='Force the archive step.',
      action='store_true')
  parser.add_option('--gyp',
      help='Use gyp to build examples/libraries/Makefiles.',
      action='store_true')
  parser.add_option('--release', help='PPAPI release version.',
      dest='release', default=None)
  parser.add_option('--build-ports',
      help='Build naclport bundle.', action='store_true')
  parser.add_option('--experimental',
      help='build experimental examples and libraries', action='store_true',
      dest='build_experimental')
  parser.add_option('--skip-toolchain', help='Skip toolchain untar',
      action='store_true')
  parser.add_option('--mac_sdk',
      help='Set the mac_sdk (e.g. 10.6) to use when building with ninja.',
      dest='mac_sdk')

  global options
  options, args = parser.parse_args(args[1:])
  platform = getos.GetPlatform()
  arch = 'x86'

  generate_make.use_gyp = options.gyp
  if buildbot_common.IsSDKBuilder():
    options.run_tests = True
    options.archive = True
    options.build_ports = True

  if buildbot_common.IsSDKTrybot():
    options.run_tests = True

  toolchains = ['newlib', 'glibc', 'arm', 'pnacl', 'host']
  print 'Building: ' + ' '.join(toolchains)

  if options.archive and options.skip_tar:
    parser.error('Incompatible arguments with archive.')

  chrome_version = int(build_version.ChromeMajorVersion())
  clnumber = build_version.ChromeRevision()
  pepper_ver = str(chrome_version)
  pepper_old = str(chrome_version - 1)
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  pepperdir_old = os.path.join(OUT_DIR, 'pepper_' + pepper_old)
  tarname = 'naclsdk_' + platform + '.tar.bz2'
  tarfile = os.path.join(OUT_DIR, tarname)

  if options.release:
    pepper_ver = options.release
  print 'Building PEPPER %s at %s' % (pepper_ver, clnumber)

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # of the build.
    del os.environ['NACL_SDK_ROOT']

  if options.run_tests:
    BuildStepRunUnittests()

  BuildStepCleanPepperDirs(pepperdir, pepperdir_old)
  BuildStepMakePepperDirs(pepperdir, ['include', 'toolchain', 'tools'])

  if not options.skip_toolchain:
    BuildStepDownloadToolchains()
    BuildStepUntarToolchains(pepperdir, platform, arch, toolchains)

  BuildStepCopyTextFiles(pepperdir, pepper_ver, clnumber)
  BuildStepBuildToolchains(pepperdir, platform, toolchains)
  InstallCommonHeaders(os.path.join(pepperdir, 'include'))

  BuildStepUpdateHelpers(pepperdir, platform, True)
  BuildStepUpdateUserProjects(pepperdir, platform, toolchains,
                              options.build_experimental, True)

  # Ship with libraries prebuilt, so run that first.
  BuildStepBuildLibraries(pepperdir, platform, 'src')
  GenerateNotice(pepperdir)

  if not options.skip_tar:
    BuildStepTarBundle(pepper_ver, tarfile)

  if options.build_ports and platform == 'linux':
    ports_tarfile = os.path.join(OUT_DIR, 'naclports.tar.bz2')
    BuildStepSyncNaClPorts()
    BuildStepBuildNaClPorts(pepper_ver, pepperdir)
    if not options.skip_tar:
      BuildStepTarNaClPorts(pepper_ver, ports_tarfile)

  if options.run_tests:
    BuildStepTestSDK()

  # Archive on non-trybots.
  if options.archive:
    BuildStepArchiveBundle('build', pepper_ver, clnumber, tarfile)
    if platform == 'linux':
      BuildStepArchiveBundle('naclports', pepper_ver, clnumber, ports_tarfile)
    BuildStepArchiveSDKTools()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
