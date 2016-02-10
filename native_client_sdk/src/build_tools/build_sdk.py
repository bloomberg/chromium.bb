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
import argparse
import datetime
import glob
import os
import re
import sys

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)

# local includes
import buildbot_common
import build_projects
import build_updater
import build_version
import generate_notice
import manifest_util
import parse_dsc
import verify_filelist

from build_paths import SCRIPT_DIR, SDK_SRC_DIR, SRC_DIR, NACL_DIR, OUT_DIR
from build_paths import GSTORE, GONACL_APPENGINE_SRC_DIR

# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))

import getos
import oshelpers

BUILD_DIR = os.path.join(NACL_DIR, 'build')
NACL_TOOLCHAIN_DIR = os.path.join(NACL_DIR, 'toolchain')
NACL_TOOLCHAINTARS_DIR = os.path.join(NACL_TOOLCHAIN_DIR, '.tars')

CYGTAR = os.path.join(BUILD_DIR, 'cygtar.py')
PKGVER = os.path.join(BUILD_DIR, 'package_version', 'package_version.py')

GYPBUILD_DIR = 'gypbuild'

options = None

# Map of: ToolchainName: (PackageName, SDKDir, arch).
TOOLCHAIN_PACKAGE_MAP = {
    'arm_glibc': ('nacl_arm_glibc', '%(platform)s_arm_glibc', 'arm'),
    'x86_glibc': ('nacl_x86_glibc', '%(platform)s_x86_glibc', 'x86'),
    'pnacl': ('pnacl_newlib', '%(platform)s_pnacl', 'pnacl')
    }


def GetToolchainDirName(tcname):
  """Return the directory name for a given toolchain"""
  return TOOLCHAIN_PACKAGE_MAP[tcname][1] % {'platform': getos.GetPlatform()}


def GetToolchainDir(pepperdir, tcname):
  """Return the full path to a given toolchain within a given sdk root"""
  return os.path.join(pepperdir, 'toolchain', GetToolchainDirName(tcname))


def GetToolchainLibc(tcname):
  if tcname == 'pnacl':
    return 'newlib'
  for libc in ('glibc', 'newlib', 'host'):
    if libc in tcname:
      return libc


def GetToolchainNaClInclude(pepperdir, tcname, arch=None):
  tcpath = GetToolchainDir(pepperdir, tcname)
  if arch is None:
    arch = TOOLCHAIN_PACKAGE_MAP[tcname][2]
  if arch == 'x86':
    return os.path.join(tcpath, 'x86_64-nacl', 'include')
  elif arch == 'pnacl':
    return os.path.join(tcpath, 'le32-nacl', 'include')
  elif arch == 'arm':
    return os.path.join(tcpath, 'arm-nacl', 'include')
  else:
    buildbot_common.ErrorExit('Unknown architecture: %s' % arch)


def GetConfigDir(arch):
  if arch.endswith('x64') and getos.GetPlatform() == 'win':
    return 'Release_x64'
  else:
    return 'Release'


def GetNinjaOutDir(arch):
  return os.path.join(OUT_DIR, GYPBUILD_DIR + '-' + arch, GetConfigDir(arch))


def GetGypBuiltLib(tcname, arch):
  if arch == 'ia32':
    lib_suffix = '32'
  elif arch == 'x64':
    lib_suffix = '64'
  elif arch == 'arm':
    lib_suffix = 'arm'
  else:
    lib_suffix = ''

  tcdir = 'tc_' + GetToolchainLibc(tcname)

  if tcname == 'pnacl':
    if arch is None:
      lib_suffix = ''
      tcdir = 'tc_pnacl_newlib'
      arch = 'x64'

  return os.path.join(GetNinjaOutDir(arch), 'gen', tcdir, 'lib' + lib_suffix)


def GetToolchainNaClLib(tcname, tcpath, arch):
  if arch == 'ia32':
    return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
  elif arch == 'x64':
    return os.path.join(tcpath, 'x86_64-nacl', 'lib')
  elif arch == 'arm':
    return os.path.join(tcpath, 'arm-nacl', 'lib')
  elif tcname == 'pnacl':
    return os.path.join(tcpath, 'le32-nacl', 'lib')



def GetOutputToolchainLib(pepperdir, tcname, arch):
  tcpath = os.path.join(pepperdir, 'toolchain', GetToolchainDirName(tcname))
  return GetToolchainNaClLib(tcname, tcpath, arch)


def GetPNaClTranslatorLib(tcpath, arch):
  if arch not in ['arm', 'x86-32', 'x86-64']:
    buildbot_common.ErrorExit('Unknown architecture %s.' % arch)
  return os.path.join(tcpath, 'translator', arch, 'lib')


def BuildStepDownloadToolchains(toolchains):
  buildbot_common.BuildStep('Running package_version.py')
  args = [sys.executable, PKGVER, '--mode', 'nacl_core_sdk']
  args.extend(['sync', '--extract'])
  buildbot_common.Run(args, cwd=NACL_DIR)


def BuildStepCleanPepperDirs(pepperdir, pepperdir_old):
  buildbot_common.BuildStep('Clean Pepper Dirs')
  dirs_to_remove = (
      pepperdir,
      pepperdir_old,
      os.path.join(OUT_DIR, 'arm_trusted')
  )
  for dirname in dirs_to_remove:
    if os.path.exists(dirname):
      buildbot_common.RemoveDir(dirname)
  buildbot_common.MakeDir(pepperdir)


def BuildStepMakePepperDirs(pepperdir, subdirs):
  for subdir in subdirs:
    buildbot_common.MakeDir(os.path.join(pepperdir, subdir))

TEXT_FILES = [
  'AUTHORS',
  'COPYING',
  'LICENSE',
  'README.Makefiles',
  'getting_started/README',
]

def BuildStepCopyTextFiles(pepperdir, pepper_ver, chrome_revision,
                           nacl_revision):
  buildbot_common.BuildStep('Add Text Files')
  InstallFiles(SDK_SRC_DIR, pepperdir, TEXT_FILES)

  # Replace a few placeholders in README
  readme_text = open(os.path.join(SDK_SRC_DIR, 'README')).read()
  readme_text = readme_text.replace('${VERSION}', pepper_ver)
  readme_text = readme_text.replace('${CHROME_REVISION}', chrome_revision)
  readme_text = readme_text.replace('${CHROME_COMMIT_POSITION}',
                                    build_version.ChromeCommitPosition())
  readme_text = readme_text.replace('${NACL_REVISION}', nacl_revision)

  # Year/Month/Day Hour:Minute:Second
  time_format = '%Y/%m/%d %H:%M:%S'
  readme_text = readme_text.replace('${DATE}',
      datetime.datetime.now().strftime(time_format))

  open(os.path.join(pepperdir, 'README'), 'w').write(readme_text)


def BuildStepUntarToolchains(pepperdir, toolchains):
  buildbot_common.BuildStep('Untar Toolchains')
  platform = getos.GetPlatform()
  build_platform = '%s_x86' % platform
  tmpdir = os.path.join(OUT_DIR, 'tc_temp')
  buildbot_common.RemoveDir(tmpdir)
  buildbot_common.MakeDir(tmpdir)

  # Create a list of extract packages tuples, the first part should be
  # "$PACKAGE_TARGET/$PACKAGE". The second part should be the destination
  # directory relative to pepperdir/toolchain.
  extract_packages = []
  for toolchain in toolchains:
    toolchain_map = TOOLCHAIN_PACKAGE_MAP.get(toolchain, None)
    if toolchain_map:
      package_name, tcdir, _ = toolchain_map
      package_tuple = (os.path.join(build_platform, package_name),
                       tcdir % {'platform': platform})
      extract_packages.append(package_tuple)


  # On linux we also want to extract the arm_trusted package which contains
  # the ARM libraries we ship in support of sel_ldr_arm.
  if platform == 'linux':
    extract_packages.append((os.path.join(build_platform, 'arm_trusted'),
                            'arm_trusted'))
  if extract_packages:
    # Extract all of the packages into the temp directory.
    package_names = [package_tuple[0] for package_tuple in extract_packages]
    buildbot_common.Run([sys.executable, PKGVER,
                           '--packages', ','.join(package_names),
                           '--tar-dir', NACL_TOOLCHAINTARS_DIR,
                           '--dest-dir', tmpdir,
                           'extract'])

    # Move all the packages we extracted to the correct destination.
    for package_name, dest_dir in extract_packages:
      full_src_dir = os.path.join(tmpdir, package_name)
      full_dst_dir = os.path.join(pepperdir, 'toolchain', dest_dir)
      buildbot_common.Move(full_src_dir, full_dst_dir)

  # Cleanup the temporary directory we are no longer using.
  buildbot_common.RemoveDir(tmpdir)


# List of toolchain headers to install.
# Source is relative to top of Chromium tree, destination is relative
# to the toolchain header directory.
NACL_HEADER_MAP = {
  'newlib': [
      ('native_client/src/include/nacl/nacl_exception.h', 'nacl/'),
      ('native_client/src/include/nacl/nacl_minidump.h', 'nacl/'),
      ('native_client/src/untrusted/irt/irt.h', ''),
      ('native_client/src/untrusted/irt/irt_dev.h', ''),
      ('native_client/src/untrusted/irt/irt_extension.h', ''),
      ('native_client/src/untrusted/nacl/nacl_dyncode.h', 'nacl/'),
      ('native_client/src/untrusted/nacl/nacl_startup.h', 'nacl/'),
      ('native_client/src/untrusted/pthread/pthread.h', ''),
      ('native_client/src/untrusted/pthread/semaphore.h', ''),
      ('native_client/src/untrusted/valgrind/dynamic_annotations.h', 'nacl/'),
      ('ppapi/nacl_irt/public/irt_ppapi.h', ''),
  ],
  'glibc': [
      ('native_client/src/include/nacl/nacl_exception.h', 'nacl/'),
      ('native_client/src/include/nacl/nacl_minidump.h', 'nacl/'),
      ('native_client/src/untrusted/irt/irt.h', ''),
      ('native_client/src/untrusted/irt/irt_dev.h', ''),
      ('native_client/src/untrusted/irt/irt_extension.h', ''),
      ('native_client/src/untrusted/nacl/nacl_dyncode.h', 'nacl/'),
      ('native_client/src/untrusted/nacl/nacl_startup.h', 'nacl/'),
      ('native_client/src/untrusted/valgrind/dynamic_annotations.h', 'nacl/'),
      ('ppapi/nacl_irt/public/irt_ppapi.h', ''),
  ],
}

def InstallFiles(src_root, dest_root, file_list):
  """Copy a set of files from src_root to dest_root according
  to the given mapping.  This allows files to be copied from
  to a location in the destination tree that is different to the
  location in the source tree.

  If the destination mapping ends with a '/' then the destination
  basename is inherited from the the source file.

  Wildcards can be used in the source list but it is not recommended
  as this can end up adding things to the SDK unintentionally.
  """
  for file_spec in file_list:
    # The list of files to install can be a simple list of
    # strings or a list of pairs, where each pair corresponds
    # to a mapping from source to destination names.
    if type(file_spec) == str:
      src_file = dest_file = file_spec
    else:
      src_file, dest_file = file_spec

    src_file = os.path.join(src_root, src_file)

    # Expand sources files using glob.
    sources = glob.glob(src_file)
    if not sources:
      sources = [src_file]

    if len(sources) > 1 and not dest_file.endswith('/'):
      buildbot_common.ErrorExit("Target file must end in '/' when "
                                "using globbing to install multiple files")

    for source in sources:
      if dest_file.endswith('/'):
        dest = os.path.join(dest_file, os.path.basename(source))
      else:
        dest = dest_file
      dest = os.path.join(dest_root, dest)
      if not os.path.isdir(os.path.dirname(dest)):
        buildbot_common.MakeDir(os.path.dirname(dest))
      buildbot_common.CopyFile(source, dest)


def InstallNaClHeaders(tc_dst_inc, tcname):
  """Copies NaCl headers to expected locations in the toolchain."""
  InstallFiles(SRC_DIR, tc_dst_inc, NACL_HEADER_MAP[GetToolchainLibc(tcname)])


def MakeNinjaRelPath(path):
  return os.path.join(os.path.relpath(OUT_DIR, SRC_DIR), path)


TOOLCHAIN_LIBS = {
  'newlib' : [
    'libppapi.a',
    'libppapi_stub.a',
  ],
  'glibc': [
    'libppapi.a',
    'libppapi.so',
    'libppapi_stub.a',
  ]
}


def GypNinjaInstall(pepperdir, toolchains):
  tools_files_32 = [
    ['sel_ldr', 'sel_ldr_x86_32'],
    ['irt_core_newlib_x32.nexe', 'irt_core_x86_32.nexe'],
    ['irt_core_newlib_x64.nexe', 'irt_core_x86_64.nexe'],
  ]
  arm_files = [
    ['elf_loader_newlib_arm.nexe', 'elf_loader_arm.nexe'],
  ]

  tools_files_64 = []

  platform = getos.GetPlatform()

  # TODO(binji): dump_syms doesn't currently build on Windows. See
  # http://crbug.com/245456
  if platform != 'win':
    tools_files_64 += [
      ['dump_syms', 'dump_syms'],
      ['minidump_dump', 'minidump_dump'],
      ['minidump_stackwalk', 'minidump_stackwalk']
    ]

  tools_files_64.append(['sel_ldr', 'sel_ldr_x86_64'])
  tools_files_64.append(['ncval_new', 'ncval'])

  if platform == 'linux':
    tools_files_32.append(['nacl_helper_bootstrap',
                           'nacl_helper_bootstrap_x86_32'])
    tools_files_64.append(['nacl_helper_bootstrap',
                           'nacl_helper_bootstrap_x86_64'])
    tools_files_32.append(['nonsfi_loader_newlib_x32_nonsfi.nexe',
                           'nonsfi_loader_x86_32'])

  tools_dir = os.path.join(pepperdir, 'tools')
  buildbot_common.MakeDir(tools_dir)

  # Add .exe extensions to all windows tools
  for pair in tools_files_32 + tools_files_64:
    if platform == 'win' and not pair[0].endswith('.nexe'):
      pair[0] += '.exe'
      pair[1] += '.exe'

  # Add ARM binaries
  if platform == 'linux' and not options.no_arm_trusted:
    arm_files += [
      ['irt_core_newlib_arm.nexe', 'irt_core_arm.nexe'],
      ['nacl_helper_bootstrap', 'nacl_helper_bootstrap_arm'],
      ['nonsfi_loader_newlib_arm_nonsfi.nexe', 'nonsfi_loader_arm'],
      ['sel_ldr', 'sel_ldr_arm']
    ]

  InstallFiles(GetNinjaOutDir('x64'), tools_dir, tools_files_64)
  InstallFiles(GetNinjaOutDir('ia32'), tools_dir, tools_files_32)
  InstallFiles(GetNinjaOutDir('arm'), tools_dir, arm_files)

  for tc in toolchains:
    if tc in ('host', 'clang-newlib'):
      continue
    elif tc == 'pnacl':
      xarches = (None, 'ia32', 'x64', 'arm')
    elif tc in ('x86_glibc'):
      xarches = ('ia32', 'x64')
    elif tc == 'arm_glibc':
      xarches = ('arm',)
    else:
      raise AssertionError('unexpected toolchain value: %s' % tc)

    for xarch in xarches:
      src_dir = GetGypBuiltLib(tc, xarch)
      dst_dir = GetOutputToolchainLib(pepperdir, tc, xarch)
      libc = GetToolchainLibc(tc)
      InstallFiles(src_dir, dst_dir, TOOLCHAIN_LIBS[libc])


def GypNinjaBuild_NaCl(rel_out_dir):
  gyp_py = os.path.join(NACL_DIR, 'build', 'gyp_nacl')
  nacl_core_sdk_gyp = os.path.join(NACL_DIR, 'build', 'nacl_core_sdk.gyp')
  all_gyp = os.path.join(NACL_DIR, 'build', 'all.gyp')

  out_dir_32 = MakeNinjaRelPath(rel_out_dir + '-ia32')
  out_dir_64 = MakeNinjaRelPath(rel_out_dir + '-x64')
  out_dir_arm = MakeNinjaRelPath(rel_out_dir + '-arm')

  GypNinjaBuild('ia32', gyp_py, nacl_core_sdk_gyp, 'nacl_core_sdk', out_dir_32)
  GypNinjaBuild('x64', gyp_py, nacl_core_sdk_gyp, 'nacl_core_sdk', out_dir_64)
  GypNinjaBuild('arm', gyp_py, nacl_core_sdk_gyp, 'nacl_core_sdk', out_dir_arm)
  GypNinjaBuild('x64', gyp_py, all_gyp, 'ncval_new', out_dir_64)


def GypNinjaBuild_Breakpad(rel_out_dir):
  # TODO(binji): dump_syms doesn't currently build on Windows. See
  # http://crbug.com/245456
  if getos.GetPlatform() == 'win':
    return

  gyp_py = os.path.join(SRC_DIR, 'build', 'gyp_chromium')
  out_dir = MakeNinjaRelPath(rel_out_dir)
  gyp_file = os.path.join(SRC_DIR, 'breakpad', 'breakpad.gyp')
  build_list = ['dump_syms', 'minidump_dump', 'minidump_stackwalk']
  GypNinjaBuild('x64', gyp_py, gyp_file, build_list, out_dir)


def GypNinjaBuild_PPAPI(arch, rel_out_dir):
  gyp_py = os.path.join(SRC_DIR, 'build', 'gyp_chromium')
  out_dir = MakeNinjaRelPath(rel_out_dir)
  gyp_file = os.path.join(SRC_DIR, 'ppapi', 'native_client',
                          'native_client.gyp')
  GypNinjaBuild(arch, gyp_py, gyp_file, 'ppapi_lib', out_dir)


def GypNinjaBuild_Pnacl(arch, rel_out_dir):
  # TODO(binji): This will build the pnacl_irt_shim twice; once as part of the
  # Chromium build, and once here. When we move more of the SDK build process
  # to gyp, we can remove this.
  gyp_py = os.path.join(SRC_DIR, 'build', 'gyp_chromium')

  out_dir = MakeNinjaRelPath(rel_out_dir)
  gyp_file = os.path.join(SRC_DIR, 'ppapi', 'native_client', 'src',
                          'untrusted', 'pnacl_irt_shim', 'pnacl_irt_shim.gyp')
  GypNinjaBuild(arch, gyp_py, gyp_file, 'aot', out_dir)


def GypNinjaBuild(arch, gyp_py_script, gyp_file, targets, out_dir):
  gyp_env = dict(os.environ)
  gyp_defines = []
  if options.mac_sdk:
    gyp_defines.append('mac_sdk=%s' % options.mac_sdk)

  if arch is not None:
    gyp_defines.append('target_arch=%s' % arch)
    if arch == 'arm':
      gyp_env['GYP_CROSSCOMPILE'] = '1'
      if options.no_arm_trusted:
        gyp_defines.append('disable_cross_trusted=1')

  gyp_env['GYP_DEFINES'] = ' '.join(gyp_defines)
  # We can't use windows path separators in GYP_GENERATOR_FLAGS since
  # gyp uses shlex to parse them and treats '\' as an escape char.
  gyp_env['GYP_GENERATOR_FLAGS'] = 'output_dir=%s' % out_dir.replace('\\', '/')

  # Print relevant environment variables
  for key, value in gyp_env.iteritems():
    if key.startswith('GYP') or key in ('CC',):
      print '  %s="%s"' % (key, value)

  buildbot_common.Run(
      [sys.executable, gyp_py_script, gyp_file, '--depth=.'],
      cwd=SRC_DIR,
      env=gyp_env)

  NinjaBuild(targets, out_dir, arch)


def NinjaBuild(targets, out_dir, arch):
  if type(targets) is not list:
    targets = [targets]
  out_config_dir = os.path.join(out_dir, GetConfigDir(arch))
  buildbot_common.Run(['ninja', '-C', out_config_dir] + targets, cwd=SRC_DIR)


def BuildStepBuildToolchains(pepperdir, toolchains, build, clean):
  buildbot_common.BuildStep('SDK Items')

  if clean:
    for dirname in glob.glob(os.path.join(OUT_DIR, GYPBUILD_DIR + '*')):
      buildbot_common.RemoveDir(dirname)
    build = True

  if build:
    GypNinjaBuild_NaCl(GYPBUILD_DIR)
    GypNinjaBuild_Breakpad(GYPBUILD_DIR + '-x64')

    if 'x86_glibc' in toolchains or 'pnacl' in toolchains:
      GypNinjaBuild_PPAPI('ia32', GYPBUILD_DIR + '-ia32')
      GypNinjaBuild_PPAPI('x64', GYPBUILD_DIR + '-x64')

    if 'arm_glibc' in toolchains or 'pnacl' in toolchains:
      GypNinjaBuild_PPAPI('arm', GYPBUILD_DIR + '-arm')

    if 'pnacl' in toolchains:
      # NOTE: For ia32, gyp builds both x86-32 and x86-64 by default.
      for arch in ('ia32', 'arm'):
        # Fill in the latest native pnacl shim library from the chrome build.
        build_dir = GYPBUILD_DIR + '-pnacl-' + arch
        GypNinjaBuild_Pnacl(arch, build_dir)

  GypNinjaInstall(pepperdir, toolchains)

  for toolchain in toolchains:
    if toolchain not in ('host', 'clang-newlib'):
      InstallNaClHeaders(GetToolchainNaClInclude(pepperdir, toolchain),
                         toolchain)


  if 'pnacl' in toolchains:
    # NOTE: For ia32, gyp builds both x86-32 and x86-64 by default.
    for arch in ('ia32', 'arm'):
      # Fill in the latest native pnacl shim library from the chrome build.
      build_dir = GYPBUILD_DIR + '-pnacl-' + arch
      if arch == 'ia32':
        nacl_arches = ['x86-32', 'x86-64']
      elif arch == 'arm':
        nacl_arches = ['arm']
      else:
        buildbot_common.ErrorExit('Unknown architecture: %s' % arch)
      for nacl_arch in nacl_arches:
        release_build_dir = os.path.join(OUT_DIR, build_dir, 'Release',
                                         'gen', 'tc_pnacl_translate',
                                         'lib-' + nacl_arch)

        pnacldir = GetToolchainDir(pepperdir, 'pnacl')
        pnacl_translator_lib_dir = GetPNaClTranslatorLib(pnacldir, nacl_arch)
        if not os.path.isdir(pnacl_translator_lib_dir):
          buildbot_common.ErrorExit('Expected %s directory to exist.' %
                                    pnacl_translator_lib_dir)

        buildbot_common.CopyFile(
            os.path.join(release_build_dir, 'libpnacl_irt_shim.a'),
            pnacl_translator_lib_dir)

    InstallNaClHeaders(GetToolchainNaClInclude(pepperdir, 'pnacl', 'x86'),
                       'pnacl')
    InstallNaClHeaders(GetToolchainNaClInclude(pepperdir, 'pnacl', 'arm'),
                       'pnacl')


def MakeDirectoryOrClobber(pepperdir, dirname, clobber):
  dirpath = os.path.join(pepperdir, dirname)
  if clobber:
    buildbot_common.RemoveDir(dirpath)
  buildbot_common.MakeDir(dirpath)

  return dirpath


def BuildStepUpdateHelpers(pepperdir, clobber):
  buildbot_common.BuildStep('Update project helpers')
  build_projects.UpdateHelpers(pepperdir, clobber=clobber)


def BuildStepUpdateUserProjects(pepperdir, toolchains,
                                build_experimental, clobber):
  buildbot_common.BuildStep('Update examples and libraries')

  filters = {}
  if not build_experimental:
    filters['EXPERIMENTAL'] = False

  dsc_toolchains = []
  for t in toolchains:
    if t.startswith('x86_') or t.startswith('arm_'):
      if t[4:] not in dsc_toolchains:
        dsc_toolchains.append(t[4:])
    elif t == 'host':
      dsc_toolchains.append(getos.GetPlatform())
    else:
      dsc_toolchains.append(t)

  filters['TOOLS'] = dsc_toolchains

  # Update examples and libraries
  filters['DEST'] = [
    'getting_started',
    'examples/api',
    'examples/demo',
    'examples/tutorial',
    'src'
  ]

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=filters)
  build_projects.UpdateProjects(pepperdir, tree, clobber=clobber,
                                toolchains=dsc_toolchains)


def BuildStepMakeAll(pepperdir, directory, step_name,
                     deps=True, clean=False, config='Debug', args=None):
  buildbot_common.BuildStep(step_name)
  build_projects.BuildProjectsBranch(pepperdir, directory, clean,
                                     deps, config, args)


def BuildStepBuildLibraries(pepperdir, directory):
  BuildStepMakeAll(pepperdir, directory, 'Build Libraries Debug',
      clean=True, config='Debug')
  BuildStepMakeAll(pepperdir, directory, 'Build Libraries Release',
      clean=True, config='Release')

  # Cleanup .pyc file generated while building libraries.  Without
  # this we would end up shipping the pyc in the SDK tarball.
  buildbot_common.RemoveFile(os.path.join(pepperdir, 'tools', '*.pyc'))


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


def BuildStepVerifyFilelist(pepperdir):
  buildbot_common.BuildStep('Verify SDK Files')
  file_list_path = os.path.join(SCRIPT_DIR, 'sdk_files.list')
  try:
    print 'SDK directory: %s' % pepperdir
    verify_filelist.Verify(file_list_path, pepperdir)
    print 'OK'
  except verify_filelist.ParseException, e:
    buildbot_common.ErrorExit('Parsing sdk_files.list failed:\n\n%s' % e)
  except verify_filelist.VerifyException, e:
    file_list_rel = os.path.relpath(file_list_path)
    verify_filelist_py = os.path.splitext(verify_filelist.__file__)[0] + '.py'
    verify_filelist_py = os.path.relpath(verify_filelist_py)
    pepperdir_rel = os.path.relpath(pepperdir)

    msg = """\
SDK verification failed:

%s
Add/remove files from %s to fix.

Run:
    ./%s %s %s
to test.""" % (e, file_list_rel, verify_filelist_py, file_list_rel,
               pepperdir_rel)
    buildbot_common.ErrorExit(msg)


def BuildStepTarBundle(pepper_ver, tarfile):
  buildbot_common.BuildStep('Tar Pepper Bundle')
  buildbot_common.MakeDir(os.path.dirname(tarfile))
  buildbot_common.Run([sys.executable, CYGTAR, '-C', OUT_DIR, '-cjf', tarfile,
       'pepper_' + pepper_ver], cwd=NACL_DIR)


def GetManifestBundle(pepper_ver, chrome_revision, nacl_revision, tarfile,
                      archive_url):
  with open(tarfile, 'rb') as tarfile_stream:
    archive_sha1, archive_size = manifest_util.DownloadAndComputeHash(
        tarfile_stream)

  archive = manifest_util.Archive(manifest_util.GetHostOS())
  archive.url = archive_url
  archive.size = archive_size
  archive.checksum = archive_sha1

  bundle = manifest_util.Bundle('pepper_' + pepper_ver)
  bundle.revision = int(chrome_revision)
  bundle.repath = 'pepper_' + pepper_ver
  bundle.version = int(pepper_ver)
  bundle.description = (
      'Chrome %s bundle. Chrome revision: %s. NaCl revision: %s' % (
            pepper_ver, chrome_revision, nacl_revision))
  bundle.stability = 'dev'
  bundle.recommended = 'no'
  bundle.archives = [archive]
  return bundle


def Archive(filename, from_directory, step_link=True):
  if buildbot_common.IsSDKBuilder():
    bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/'
  else:
    bucket_path = 'nativeclient-mirror/nacl/nacl_sdk_test/'
  bucket_path += build_version.ChromeVersion()
  buildbot_common.Archive(filename, bucket_path, from_directory, step_link)


def BuildStepArchiveBundle(name, pepper_ver, chrome_revision, nacl_revision,
                           tarfile):
  buildbot_common.BuildStep('Archive %s' % name)
  tarname = os.path.basename(tarfile)
  tarfile_dir = os.path.dirname(tarfile)
  Archive(tarname, tarfile_dir)

  # generate "manifest snippet" for this archive.
  archive_url = GSTORE + 'nacl_sdk/%s/%s' % (
      build_version.ChromeVersion(), tarname)
  bundle = GetManifestBundle(pepper_ver, chrome_revision, nacl_revision,
                             tarfile, archive_url)

  manifest_snippet_file = os.path.join(OUT_DIR, tarname + '.json')
  with open(manifest_snippet_file, 'wb') as manifest_snippet_stream:
    manifest_snippet_stream.write(bundle.GetDataAsString())

  Archive(tarname + '.json', OUT_DIR, step_link=False)


def BuildStepBuildPNaClComponent(version, revision):
  # Sadly revision can go backwords for a given version since when a version
  # is built from master, revision will be a huge number (in the hundreds of
  # thousands.  Once the branch happens the revision will reset to zero.
  # TODO(sbc): figure out how to compensate for this in some way such that
  # revisions always go forward for a given version.
  buildbot_common.BuildStep('PNaCl Component')
  # Version numbers must follow the format specified in:
  # https://developer.chrome.com/extensions/manifest/version
  # So ensure that rev_major/rev_minor don't overflow and ensure there
  # are no leading zeros.
  if len(revision) > 4:
    rev_minor = int(revision[-4:])
    rev_major = int(revision[:-4])
    version = "0.%s.%s.%s" % (version, rev_major, rev_minor)
  else:
    version = "0.%s.0.%s" % (version, revision)
  buildbot_common.Run(['./make_pnacl_component.sh',
                       'pnacl_multicrx_%s.zip' % revision,
                       version], cwd=SCRIPT_DIR)


def BuildStepArchivePNaClComponent(revision):
  buildbot_common.BuildStep('Archive PNaCl Component')
  Archive('pnacl_multicrx_%s.zip' % revision, OUT_DIR)


def BuildStepArchiveSDKTools():
  buildbot_common.BuildStep('Build SDK Tools')
  build_updater.BuildUpdater(OUT_DIR)

  buildbot_common.BuildStep('Archive SDK Tools')
  Archive('sdk_tools.tgz', OUT_DIR, step_link=False)
  Archive('nacl_sdk.zip', OUT_DIR, step_link=False)


def BuildStepBuildAppEngine(pepperdir, chrome_revision):
  """Build the projects found in src/gonacl_appengine/src"""
  buildbot_common.BuildStep('Build GoNaCl AppEngine Projects')
  cmd = ['make', 'upload', 'REVISION=%s' % chrome_revision]
  env = dict(os.environ)
  env['NACL_SDK_ROOT'] = pepperdir
  env['NACLPORTS_NO_ANNOTATE'] = "1"
  buildbot_common.Run(cmd, env=env, cwd=GONACL_APPENGINE_SRC_DIR)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--qemu', help='Add qemu for ARM.',
      action='store_true')
  parser.add_argument('--tar', help='Force the tar step.',
      action='store_true')
  parser.add_argument('--archive', help='Force the archive step.',
      action='store_true')
  parser.add_argument('--release', help='PPAPI release version.',
      dest='release', default=None)
  parser.add_argument('--build-app-engine',
      help='Build AppEngine demos.', action='store_true')
  parser.add_argument('--experimental',
      help='build experimental examples and libraries', action='store_true',
      dest='build_experimental')
  parser.add_argument('--skip-toolchain', help='Skip toolchain untar',
      action='store_true')
  parser.add_argument('--no-clean', dest='clean', action='store_false',
      help="Don't clean gypbuild directories")
  parser.add_argument('--mac-sdk',
      help='Set the mac-sdk (e.g. 10.6) to use when building with ninja.')
  parser.add_argument('--no-arm-trusted', action='store_true',
      help='Disable building of ARM trusted components (sel_ldr, etc).')
  parser.add_argument('--no-use-sysroot', action='store_true',
      help='Disable building against sysroot.')

  # To setup bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete build_sdk.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  global options
  options = parser.parse_args(args)

  buildbot_common.BuildStep('build_sdk')

  if buildbot_common.IsSDKBuilder():
    options.archive = True
    # TODO(binji): re-enable app_engine build when the linux builder stops
    # breaking when trying to git clone from github.
    # See http://crbug.com/412969.
    options.build_app_engine = False
    options.tar = True

  # NOTE: order matters here. This will be the order that is specified in the
  # Makefiles; the first toolchain will be the default.
  toolchains = ['pnacl', 'x86_glibc', 'arm_glibc', 'clang-newlib', 'host']

  print 'Building: ' + ' '.join(toolchains)
  platform = getos.GetPlatform()

  if options.archive and not options.tar:
    parser.error('Incompatible arguments with archive.')

  chrome_version = int(build_version.ChromeMajorVersion())
  chrome_revision = build_version.ChromeRevision()
  nacl_revision = build_version.NaClRevision()
  pepper_ver = str(chrome_version)
  pepper_old = str(chrome_version - 1)
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  pepperdir_old = os.path.join(OUT_DIR, 'pepper_' + pepper_old)
  tarname = 'naclsdk_%s.tar.bz2' % platform
  tarfile = os.path.join(OUT_DIR, tarname)

  if options.release:
    pepper_ver = options.release
  print 'Building PEPPER %s at %s' % (pepper_ver, chrome_revision)

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # of the build.
    del os.environ['NACL_SDK_ROOT']

  if platform == 'linux':
    # Linux-only: make sure the debian/stable sysroot image is installed
    install_script = os.path.join(SRC_DIR, 'build', 'linux', 'sysroot_scripts',
                                  'install-sysroot.py')

    buildbot_common.Run([sys.executable, install_script, '--arch=arm'])
    buildbot_common.Run([sys.executable, install_script, '--arch=i386'])
    buildbot_common.Run([sys.executable, install_script, '--arch=amd64'])

  if not options.skip_toolchain:
    BuildStepCleanPepperDirs(pepperdir, pepperdir_old)
    BuildStepMakePepperDirs(pepperdir, ['include', 'toolchain', 'tools'])
    BuildStepDownloadToolchains(toolchains)
    BuildStepUntarToolchains(pepperdir, toolchains)
    if platform == 'linux':
      buildbot_common.Move(os.path.join(pepperdir, 'toolchain', 'arm_trusted'),
                           os.path.join(OUT_DIR, 'arm_trusted'))


  if platform == 'linux':
    # Linux-only: Copy arm libraries from the arm_trusted package.  These are
    # needed to be able to run sel_ldr_arm under qemu.
    arm_libs = [
      'lib/arm-linux-gnueabihf/librt.so.1',
      'lib/arm-linux-gnueabihf/libpthread.so.0',
      'lib/arm-linux-gnueabihf/libgcc_s.so.1',
      'lib/arm-linux-gnueabihf/libc.so.6',
      'lib/arm-linux-gnueabihf/ld-linux-armhf.so.3',
      'lib/arm-linux-gnueabihf/libm.so.6',
      'usr/lib/arm-linux-gnueabihf/libstdc++.so.6'
    ]
    arm_lib_dir = os.path.join(pepperdir, 'tools', 'lib', 'arm_trusted', 'lib')
    buildbot_common.MakeDir(arm_lib_dir)
    for arm_lib in arm_libs:
      arm_lib = os.path.join(OUT_DIR, 'arm_trusted', arm_lib)
      buildbot_common.CopyFile(arm_lib, arm_lib_dir)
    buildbot_common.CopyFile(os.path.join(OUT_DIR, 'arm_trusted', 'qemu-arm'),
                             os.path.join(pepperdir, 'tools'))


  BuildStepBuildToolchains(pepperdir, toolchains,
                           not options.skip_toolchain,
                           options.clean)

  BuildStepUpdateHelpers(pepperdir, True)
  BuildStepUpdateUserProjects(pepperdir, toolchains,
                              options.build_experimental, True)

  BuildStepCopyTextFiles(pepperdir, pepper_ver, chrome_revision, nacl_revision)

  # Ship with libraries prebuilt, so run that first.
  BuildStepBuildLibraries(pepperdir, 'src')
  GenerateNotice(pepperdir)

  # Verify the SDK contains what we expect.
  BuildStepVerifyFilelist(pepperdir)

  if options.tar:
    BuildStepTarBundle(pepper_ver, tarfile)

  if platform == 'linux':
    BuildStepBuildPNaClComponent(pepper_ver, chrome_revision)

  if options.build_app_engine and platform == 'linux':
    BuildStepBuildAppEngine(pepperdir, chrome_revision)

  if options.qemu:
    qemudir = os.path.join(NACL_DIR, 'toolchain', 'linux_arm-trusted')
    oshelpers.Copy(['-r', qemudir, pepperdir])

  # Archive the results on Google Cloud Storage.
  if options.archive:
    BuildStepArchiveBundle('build', pepper_ver, chrome_revision, nacl_revision,
                           tarfile)
    # Only archive sdk_tools/naclport/pnacl_component on linux.
    if platform == 'linux':
      BuildStepArchiveSDKTools()
      BuildStepArchivePNaClComponent(chrome_revision)

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('build_sdk: interrupted')
