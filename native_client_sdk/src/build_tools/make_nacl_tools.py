#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build NaCl tools (e.g. sel_ldr and ncval) at a given revision."""

import build_utils
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

bot = build_utils.BotAnnotator()


# The suffix used for NaCl moduels that are installed, such as irt_core.
NEXE_SUFFIX = '.nexe'

def MakeInstallDirs(options):
  '''Create the necessary install directories in the SDK staging area.
  '''
  install_dir = os.path.join(options.toolchain, 'bin');
  if not os.path.exists(install_dir):
    os.makedirs(install_dir)
  runtime_dir = os.path.join(options.toolchain, 'runtime');
  if not os.path.exists(runtime_dir):
    os.makedirs(runtime_dir)


def Build(options):
  '''Build 32-bit and 64-bit versions of needed NaCL tools and libs.'''
  nacl_dir = os.path.join(options.nacl_dir, 'native_client')
  toolchain_option = 'naclsdk_mode=custom:%s' % options.toolchain
  libc_option = '' if options.lib == 'newlib' else ' --nacl_glibc'
  if sys.platform == 'win32':
    scons = os.path.join(nacl_dir, 'scons.bat')
    bits32 = 'vcvarsall.bat x86 && '
    bits64 = 'vcvarsall.bat x86_amd64 && '
  else:
    scons = os.path.join(nacl_dir, 'scons')
    bits32 = ''
    bits64 = ''

  # Build sel_ldr and ncval.
  def BuildTools(prefix, bits, target):
    cmd = '%s%s -j %s --mode=%s platform=x86-%s naclsdk_validate=0 %s %s%s' % (
        prefix, scons, options.jobs, options.variant, bits, target,
        toolchain_option, libc_option)
    bot.Run(cmd, shell=True, cwd=nacl_dir)

  BuildTools(bits32, '32', 'sdl=none sel_ldr ncval')
  BuildTools(bits64, '64', 'sdl=none sel_ldr ncval')

  # Build irt_core, which is needed for running .nexes with sel_ldr.
  def BuildIRT(bits):
    cmd = '%s -j %s irt_core --mode=opt-host,nacl platform=x86-%s %s' % (
        scons, options.jobs, bits, toolchain_option)
    bot.Run(cmd, shell=True, cwd=nacl_dir)

  # only build the IRT using the newlib chain.  glibc does not support IRT.
  if options.lib == 'newlib':
    BuildIRT(32)
    BuildIRT(64)

  # Build and install untrusted libraries.
  def BuildAndInstallLibsAndHeaders(bits):
    cmd = ('%s install --mode=opt-host,nacl libdir=%s includedir=%s '
           'platform=x86-%s force_sel_ldr=none %s%s') % (
        scons,
        os.path.join(options.toolchain,
                     'x86_64-nacl',
                     'lib32' if bits == 32 else 'lib'),
        os.path.join(options.toolchain, 'x86_64-nacl', 'include'),
        bits,
        toolchain_option,
        libc_option)
    bot.Run(cmd, shell=True, cwd=nacl_dir)

  BuildAndInstallLibsAndHeaders(32)
  BuildAndInstallLibsAndHeaders(64)


def Install(options, tools=[], runtimes=[]):
  '''Install the NaCl tools and runtimes into the SDK staging area.

  Assumes that all necessary artifacts are built into the NaCl scons-out/staging
  directory, and copies them from there into the SDK staging area under
  toolchain.

  Args:
    options: The build options object.  This is populated from command-line
        args at start-up.
    tools: A list of tool names, these should *not* have any executable
        suffix - this utility adds that (e.g. '.exe' on Windows).
    runtimes: A list of IRT runtimes.  These artifacts should *not* have any
        suffix attached - this utility adds the '.nexe' suffix along with an
        ISA-specific string (e.g. '_x86_32').
  '''
  # TODO(bradnelson): add an 'install' alias to the main build for this.
  nacl_dir = os.path.join(options.nacl_dir, 'native_client')
  tool_build_path_32 = os.path.join(nacl_dir,
                                    'scons-out',
                                    '%s-x86-32' % (options.variant),
                                    'staging')
  tool_build_path_64 = os.path.join(nacl_dir,
                                    'scons-out',
                                    '%s-x86-64' % (options.variant),
                                    'staging')

  for nacl_tool in tools:
    shutil.copy(os.path.join(tool_build_path_32,
                             '%s%s' % (nacl_tool, options.exe_suffix)),
                os.path.join(options.toolchain,
                             'bin',
                             '%s_x86_32%s' % (nacl_tool, options.exe_suffix)))
    shutil.copy(os.path.join(tool_build_path_64,
                             '%s%s' % (nacl_tool, options.exe_suffix)),
                os.path.join(options.toolchain,
                             'bin',
                             '%s_x86_64%s' % (nacl_tool, options.exe_suffix)))

  irt_build_path_32 = os.path.join(nacl_dir,
                                   'scons-out',
                                   'nacl_irt-x86-32',
                                   'staging')
  irt_build_path_64 = os.path.join(nacl_dir,
                                    'scons-out',
                                    'nacl_irt-x86-64',
                                    'staging')
  for nacl_irt in runtimes:
    shutil.copy(os.path.join(irt_build_path_32,
                             '%s%s' % (nacl_irt, NEXE_SUFFIX)),
                os.path.join(options.toolchain,
                             'runtime',
                             '%s_x86_32%s' % (nacl_irt, NEXE_SUFFIX)))
    shutil.copy(os.path.join(irt_build_path_64,
                             '%s%s' % (nacl_irt, NEXE_SUFFIX)),
                os.path.join(options.toolchain,
                             'runtime',
                             '%s_x86_64%s' % (nacl_irt, NEXE_SUFFIX)))


def BuildNaClTools(options):
  if(options.clean):
    bot.Print('Removing scons-out')
    scons_out = os.path.join(options.nacl_dir, 'native_client', 'scons-out')
    build_utils.CleanDirectory(scons_out)
  else:
    MakeInstallDirs(options)
    Build(options)
    Install(options, tools=['sel_ldr', 'ncval'], runtimes=['irt_core'])
  return 0


def main(argv):
  if sys.platform in ['win32', 'cygwin']:
    exe_suffix = '.exe'
  else:
    exe_suffix = ''

  script_dir = os.path.abspath(os.path.dirname(__file__))

  parser = optparse.OptionParser()
  parser.add_option(
      '-t', '--toolchain', dest='toolchain',
      default='toolchain',
      help='where to put the NaCl tool binaries')
  parser.add_option(
      '-l', '--lib', dest='lib',
      default='newlib',
      help='whether to build against newlib (default) or glibc')
  parser.add_option(
      '-c', '--clean', action='store_true', dest='clean',
      default=False,
      help='whether to clean up the checkout files')
  parser.add_option(
      '-j', '--jobs', dest='jobs', default='1',
      help='Number of parallel jobs to use while building nacl tools')
  parser.add_option(
      '-n', '--nacl_dir', dest='nacl_dir',
      default=os.path.join(script_dir, 'packages', 'native_client'),
      help='Location of Native Client repository used for building tools')
  (options, args) = parser.parse_args(argv)
  if args:
    parser.print_help()
    bot.Print('ERROR: invalid argument(s): %s' % args)
    return 1

  options.toolchain = os.path.abspath(options.toolchain)
  options.exe_suffix = exe_suffix
  # Pick variant.
  if sys.platform in ['win32', 'cygwin']:
    variant = 'dbg-win'
  elif sys.platform == 'darwin':
    variant = 'dbg-mac'
  elif sys.platform in ['linux', 'linux2']:
    variant = 'dbg-linux'
  else:
    assert False
  options.variant = variant

  if options.lib not in ['newlib', 'glibc']:
    bot.Print('ERROR: --lib must either be newlib or glibc')
    return 1

  return BuildNaClTools(options)


if __name__ == '__main__':
  main(sys.argv[1:])
