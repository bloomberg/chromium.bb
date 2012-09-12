#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point for both build and try bots

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
import generate_make
import optparse
import os
import platform
import subprocess
import sys

# local includes
import buildbot_common
import build_updater
import build_utils
import manifest_util
from tests import test_server

# Create the various paths of interest
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
SDK_LIBRARY_DIR = os.path.join(SDK_SRC_DIR, 'libraries')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')
OUT_DIR = os.path.join(SRC_DIR, 'out')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')
SERVER_DIR = os.path.join(OUT_DIR, 'local_server')


# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))

import getos
import http_download
import oshelpers

GSTORE = 'https://commondatastorage.googleapis.com/nativeclient-mirror/nacl/'
MAKE = 'nacl_sdk/make_3_81/make.exe'
CYGTAR = os.path.join(NACL_DIR, 'build', 'cygtar.py')


options = None


def BuildOutputDir(*paths):
  return os.path.join(OUT_DIR, *paths)


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
  # Refine the toolchain host arch.  For linux, we happen to have
  # toolchains for 64-bit hosts.  For other OSes, we only have 32-bit binaries.
  arch = 'x86_32'
  if os_platform == 'linux' and platform.machine() == 'x86_64':
    arch = 'x86_64'
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


def GetToolchainNaClInclude(tcname, tcpath, arch, xarch=None):
  if arch == 'x86':
    if tcname == 'pnacl':
      return os.path.join(tcpath, 'newlib', 'sdk', 'include')
    return os.path.join(tcpath, 'x86_64-nacl', 'include')
  else:
    buildbot_common.ErrorExit('Unknown architecture.')


def GetToolchainNaClLib(tcname, tcpath, arch, xarch):
  if arch == 'x86':
    if tcname == 'pnacl':
      return os.path.join(tcpath, 'newlib', 'sdk', 'lib')
    if str(xarch) == '32':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
    if str(xarch) == '64':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib')
  buildbot_common.ErrorExit('Unknown architecture.')


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


def BuildStepBuildToolsTests():
  buildbot_common.BuildStep('Run build_tools tests')
  buildbot_common.Run([sys.executable,
      os.path.join(SDK_SRC_DIR, 'build_tools', 'tests', 'test_all.py')])


def BuildStepDownloadToolchains(platform):
  buildbot_common.BuildStep('Rerun hooks to get toolchains')
  buildbot_common.Run(['gclient', 'runhooks'],
                      cwd=SRC_DIR, shell=(platform=='win'))


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
  files = ['AUTHORS', 'COPYING', 'LICENSE', 'NOTICE']
  files = [os.path.join(SDK_SRC_DIR, filename) for filename in files]
  oshelpers.Copy(['-v'] + files + [pepperdir])

  # Replace a few placeholders in README
  readme_text = open(os.path.join(SDK_SRC_DIR, 'README'), 'rt').read()
  readme_text = readme_text.replace('${VERSION}', pepper_ver)
  readme_text = readme_text.replace('${REVISION}', revision)

  # Year/Month/Day Hour:Minute:Second
  time_format = '%Y/%m/%d %H:%M:%S'
  readme_text = readme_text.replace('${DATE}',
      datetime.datetime.now().strftime(time_format))

  open(os.path.join(pepperdir, 'README'), 'wt').write(readme_text)


def BuildStepUntarToolchains(pepperdir, platform, arch, toolchains):
  buildbot_common.BuildStep('Untar Toolchains')
  tcname = platform + '_' + arch
  tmpdir = os.path.join(SRC_DIR, 'out', 'tc_temp')
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
    compiler_dir = os.path.join(pepperdir, 'toolchain', tcname, 'bin')
    wrapper = os.path.join(SDK_SRC_DIR, 'tools', 'compiler-wrapper.py')
    buildbot_common.MakeDir(compiler_dir)
    buildbot_common.CopyFile(wrapper, compiler_dir)

    os.symlink('compiler-wrapper.py',
               os.path.join(compiler_dir, 'i686-nacl-g++'))
    os.symlink('compiler-wrapper.py',
               os.path.join(compiler_dir, 'i686-nacl-gcc'))
    os.symlink('compiler-wrapper.py',
               os.path.join(compiler_dir, 'i686-nacl-ar'))


HEADER_MAP = {
  'newlib': {
      'pthread.h': 'src/untrusted/pthread/pthread.h',
      'semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'nacl/nacl_dyncode.h': 'src/untrusted/nacl/nacl_dyncode.h',
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
      'nacl/nacl_startup.h': 'src/untrusted/nacl/nacl_startup.h',
      'nacl/nacl_thread.h': 'src/untrusted/nacl/nacl_thread.h',
      'pnacl.h': 'src/untrusted/nacl/pnacl.h',
      'irt.h': 'src/untrusted/irt/irt.h',
      'irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
  'libs': {
  },
}


def InstallHeaders(tc_dst_inc, pepper_ver, tc_name):
  """Copies NaCl headers to expected locations in the toolchain."""
  tc_map = HEADER_MAP[tc_name]
  for filename in tc_map:
    src = os.path.join(NACL_DIR, tc_map[filename])
    dst = os.path.join(tc_dst_inc, filename)
    buildbot_common.MakeDir(os.path.dirname(dst))
    buildbot_common.CopyFile(src, dst)

  # Clean out per toolchain ppapi directory
  ppapi = os.path.join(tc_dst_inc, 'ppapi')
  buildbot_common.RemoveDir(ppapi)

  # Copy in c and c/dev headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'c', 'dev'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'c', '*.h'),
          os.path.join(ppapi, 'c'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'c', 'dev', '*.h'),
          os.path.join(ppapi, 'c', 'dev'))

  # Run the generator to overwrite IDL files
  generator_args = [sys.executable, 'generator.py', '--wnone', '--cgen',
      '--verbose', '--dstroot=%s/c' % ppapi]
  if pepper_ver:
    generator_args.append('--release=M' + pepper_ver)
  buildbot_common.Run(generator_args, cwd=os.path.join(PPAPI_DIR, 'generators'))

  # Remove private and trusted interfaces
  buildbot_common.RemoveDir(os.path.join(ppapi, 'c', 'private'))
  buildbot_common.RemoveDir(os.path.join(ppapi, 'c', 'trusted'))

  # Copy in the C++ headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'cpp', 'dev'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'cpp','*.h'),
          os.path.join(ppapi, 'cpp'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'cpp', 'dev', '*.h'),
          os.path.join(ppapi, 'cpp', 'dev'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'graphics'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'threading'))
  buildbot_common.MakeDir(os.path.join(ppapi, 'utility', 'websocket'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'utility','*.h'),
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
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'EGL'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'EGL', '*.h'),
          os.path.join(tc_dst_inc, 'EGL'))

  # Copy the GLES2 headers
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'GLES2'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'GLES2', '*.h'),
          os.path.join(tc_dst_inc, 'GLES2'))

  # Copy the KHR headers
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'KHR'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR, 'lib', 'gl', 'include', 'KHR', '*.h'),
          os.path.join(tc_dst_inc, 'KHR'))

  # Copy the lib files
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'lib'),
          os.path.join(tc_dst_inc, 'ppapi'))


def BuildStepBuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains):
  buildbot_common.BuildStep('SDK Items')

  tcname = platform + '_' + arch
  newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
  glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
  pnacldir = os.path.join(pepperdir, 'toolchain', tcname + '_pnacl')

  # Run scons TC build steps
  if arch == 'x86':
    if 'newlib' in toolchains:
      buildbot_common.Run(
          GetBuildArgs('newlib', newlibdir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      buildbot_common.Run(
          GetBuildArgs('newlib', newlibdir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      InstallHeaders(GetToolchainNaClInclude('newlib', newlibdir, 'x86'),
                     pepper_ver,
                     'newlib')

    if 'glibc' in toolchains:
      buildbot_common.Run(
          GetBuildArgs('glibc', glibcdir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      buildbot_common.Run(
          GetBuildArgs('glibc', glibcdir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      InstallHeaders(GetToolchainNaClInclude('glibc', glibcdir, 'x86'),
                     pepper_ver,
                     'glibc')

    if 'pnacl' in toolchains:
      buildbot_common.Run(
          GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      buildbot_common.Run(
          GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      # Pnacl libraries are typically bitcode, but some are native and
      # will be looked up in the native library directory.
      buildbot_common.Move(
          os.path.join(GetToolchainNaClLib('pnacl', pnacldir, 'x86', '64'),
                       'libpnacl_irt_shim.a'),
          GetPNaClNativeLib(pnacldir, 'x86-64'))
      InstallHeaders(GetToolchainNaClInclude('pnacl', pnacldir, 'x86'),
                     pepper_ver,
                     'newlib')
  else:
    buildbot_common.ErrorExit('Missing arch %s' % arch)


def BuildStepCopyBuildHelpers(pepperdir, platform):
  buildbot_common.BuildStep('Copy build helpers')
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
      os.path.join(pepperdir, 'tools'))
  if platform == 'win':
    buildbot_common.BuildStep('Add MAKE')
    http_download.HttpDownload(GSTORE + MAKE,
                               os.path.join(pepperdir, 'tools' ,'make.exe'))
    rename_list = ['ncval_x86_32', 'ncval_x86_64',
                   'sel_ldr_x86_32', 'sel_ldr_x86_64']
    for name in rename_list:
      src = os.path.join(pepperdir, 'tools', name)
      dst = os.path.join(pepperdir, 'tools', name + '.exe')
      buildbot_common.Move(src, dst)


EXAMPLE_LIST = [
  'debugging',
  'file_histogram',
  'file_io',
  'fullscreen_tumbler',
  'gamepad',
  'geturl',
  'hello_world_interactive',
  'hello_world',
  'hello_world_gles',
  'input_events',
  'load_progress',
  'mouselock',
  'multithreaded_input_events',
  'pi_generator',
  'pong',
  'sine_synth',
  'tumbler',
  'websocket',
  'dlopen',
]

LIBRARY_LIST = [
  'nacl_mounts',
  'ppapi',
  'ppapi_cpp',
  'ppapi_gles2',
  'pthread',
]

LIB_DICT = {
  'linux': [],
  'mac': [],
  'win': ['x86_32']
}

def BuildStepCopyExamples(pepperdir, toolchains, build_experimental):
  buildbot_common.BuildStep('Copy examples')

  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    buildbot_common.ErrorExit('Examples depend on missing toolchains.')

  exampledir = os.path.join(pepperdir, 'examples')
  buildbot_common.RemoveDir(exampledir)
  buildbot_common.MakeDir(exampledir)

  libdir = os.path.join(pepperdir, 'lib')
  buildbot_common.RemoveDir(libdir)
  buildbot_common.MakeDir(libdir)

  plat = getos.GetPlatform()
  for arch in LIB_DICT[plat]:
    buildbot_common.MakeDir(os.path.join(libdir, '%s_%s_host' % (plat, arch)))
    for config in ['Debug', 'Release']:
      buildbot_common.MakeDir(os.path.join(libdir, '%s_%s_host' % (plat, arch),
                              config))


  srcdir = os.path.join(pepperdir, 'src')
  buildbot_common.RemoveDir(srcdir)
  buildbot_common.MakeDir(srcdir)


  # Copy individual files
  files = ['favicon.ico', 'httpd.cmd', 'httpd.py', 'index.html']
  for filename in files:
    oshelpers.Copy(['-v', os.path.join(SDK_EXAMPLE_DIR, filename), exampledir])

  args = ['--dstroot=%s' % pepperdir, '--master']
  for toolchain in toolchains:
    args.append('--' + toolchain)

  for example in EXAMPLE_LIST:
    dsc = os.path.join(SDK_EXAMPLE_DIR, example, 'example.dsc')
    args.append(dsc)

  for library in LIBRARY_LIST:
    dsc = os.path.join(SDK_LIBRARY_DIR, library, 'library.dsc')
    args.append(dsc)

  if build_experimental:
    args.append('--experimental')

  if generate_make.main(args):
    buildbot_common.ErrorExit('Failed to build examples.')


def GetWindowsEnvironment():
  sys.path.append(os.path.join(NACL_DIR, 'buildbot'))
  import buildbot_standard

  # buildbot_standard.SetupWindowsEnvironment expects a "context" object. We'll
  # fake enough of that here to work.
  class FakeContext(object):
    def __init__(self):
      self.env = os.environ

    def GetEnv(self, key):
      return self.env[key]

    def __getitem__(self, key):
      return self.env[key]

    def SetEnv(self, key, value):
      self.env[key] = value

    def __setitem__(self, key, value):
      self.env[key] = value

  context = FakeContext()
  buildbot_standard.SetupWindowsEnvironment(context)

  # buildbot_standard.SetupWindowsEnvironment adds the directory which contains
  # vcvarsall.bat to the path, but not the directory which contains cl.exe,
  # link.exe, etc.
  # Running vcvarsall.bat adds the correct directories to the path, which we
  # extract below.
  process = subprocess.Popen('vcvarsall.bat x86 > NUL && set',
      stdout=subprocess.PIPE, env=context.env, shell=True)
  stdout, _ = process.communicate()

  # Parse environment from "set" command above.
  # It looks like this:
  # KEY1=VALUE1\r\n
  # KEY2=VALUE2\r\n
  # ...
  return dict(line.split('=') for line in stdout.split('\r\n')[:-1])


def BuildStepMakeAll(pepperdir, platform, directory, step_name, clean=False):
  buildbot_common.BuildStep(step_name)
  make_dir = os.path.join(pepperdir, directory)
  makefile = os.path.join(make_dir, 'Makefile')
  if os.path.isfile(makefile):
    print "\n\nMake: " + make_dir
    if platform == 'win':
      # We need to modify the environment to build host on Windows.
      env = GetWindowsEnvironment()
    else:
      env = os.environ

    buildbot_common.Run(['make', '-j8'],
                        cwd=os.path.abspath(make_dir), shell=True, env=env)
    if clean:
      # Clean to remove temporary files but keep the built libraries.
      buildbot_common.Run(['make', '-j8', 'clean'],
                          cwd=os.path.abspath(make_dir), shell=True)


def BuildStepBuildLibraries(pepperdir, platform, directory):
  BuildStepMakeAll(pepperdir, platform, directory, 'Build Libraries',
      clean=True)


def BuildStepTarBundle(pepper_ver, tarfile):
  buildbot_common.BuildStep('Tar Pepper Bundle')
  buildbot_common.MakeDir(os.path.dirname(tarfile))
  buildbot_common.Run([sys.executable, CYGTAR, '-C', OUT_DIR, '-cjf', tarfile,
       'pepper_' + pepper_ver], cwd=NACL_DIR)


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


def BuildStepTestUpdater(platform, pepper_ver, revision, tarfile):
  tarname = os.path.basename(tarfile)
  server = None
  try:
    buildbot_common.BuildStep('Run local server')
    server = test_server.LocalHTTPServer(SERVER_DIR)

    buildbot_common.BuildStep('Generate manifest')
    bundle = GetManifestBundle(pepper_ver, revision, tarfile,
        server.GetURL(tarname))

    manifest = manifest_util.SDKManifest()
    manifest.SetBundle(bundle)
    manifest_name = 'naclsdk_manifest2.json'
    with open(os.path.join(SERVER_DIR, manifest_name), 'wb') as \
        manifest_stream:
      manifest_stream.write(manifest.GetDataAsString())

    # use newly built sdk updater to pull this bundle
    buildbot_common.BuildStep('Update from local server')
    naclsdk_sh = os.path.join(OUT_DIR, 'nacl_sdk', 'naclsdk')
    if platform == 'win':
      naclsdk_sh += '.bat'
    buildbot_common.Run([naclsdk_sh, '-U',
        server.GetURL(manifest_name), 'update', 'pepper_' + pepper_ver])

    # Return the new pepper directory as the one inside the downloaded SDK.
    return os.path.join(OUT_DIR, 'nacl_sdk', 'pepper_' + pepper_ver)

  # kill server
  finally:
    if server:
      server.Shutdown()


def BuildStepBuildExamples(pepperdir, platform):
  BuildStepMakeAll(pepperdir, platform, 'examples', 'Build Examples')


TEST_EXAMPLE_LIST = [
  'nacl_mounts_test',
]

TEST_LIBRARY_LIST = [
  'gtest',
  'gtest_ppapi',
]

def BuildStepCopyTests(pepperdir, toolchains, build_experimental):
  buildbot_common.BuildStep('Copy Tests')

  testingdir = os.path.join(pepperdir, 'testing')
  buildbot_common.RemoveDir(testingdir)
  buildbot_common.MakeDir(testingdir)

  args = ['--dstroot=%s' % pepperdir, '--master']
  for toolchain in toolchains:
    args.append('--' + toolchain)

  for library in TEST_LIBRARY_LIST:
    dsc = os.path.join(SDK_LIBRARY_DIR, library, 'library.dsc')
    args.append(dsc)

  for example in TEST_EXAMPLE_LIST:
    dsc = os.path.join(SDK_LIBRARY_DIR, example, 'example.dsc')
    args.append(dsc)

  if build_experimental:
    args.append('--experimental')

  if generate_make.main(args):
    buildbot_common.ErrorExit('Failed to build tests.')


def BuildStepBuildTests(pepperdir, platform):
  BuildStepMakeAll(pepperdir, platform, 'testing', 'Build Tests')


def BuildStepRunPyautoTests(pepperdir, platform, pepper_ver):
  buildbot_common.BuildStep('Test Examples')
  env = copy.copy(os.environ)
  env['PEPPER_VER'] = pepper_ver
  env['NACL_SDK_ROOT'] = pepperdir

  pyauto_script = os.path.join(SRC_DIR, 'chrome', 'test', 'functional',
      'nacl_sdk.py')
  pyauto_script_args = ['nacl_sdk.NaClSDKTest.NaClSDKExamples']

  if platform == 'linux' and buildbot_common.IsSDKBuilder():
    # linux buildbots need to run the pyauto tests through xvfb. Running
    # using runtest.py does this.
    #env['PYTHON_PATH'] = '.:' + env.get('PYTHON_PATH', '.')
    build_dir = os.path.dirname(SRC_DIR)
    runtest_py = os.path.join(build_dir, '..', '..', '..', 'scripts', 'slave',
        'runtest.py')
    buildbot_common.Run([sys.executable, runtest_py, '--target', 'Release',
        '--build-dir', 'src/build', sys.executable,
        pyauto_script] + pyauto_script_args,
        cwd=build_dir, env=env)
  else:
    buildbot_common.Run([sys.executable, 'nacl_sdk.py',
      'nacl_sdk.NaClSDKTest.NaClSDKExamples'],
      cwd=os.path.dirname(pyauto_script),
      env=env)


def BuildStepArchiveBundle(pepper_ver, revision, tarfile):
  buildbot_common.BuildStep('Archive build')
  bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s' % (
      build_utils.ChromeVersion(),)
  tarname = os.path.basename(tarfile)
  tarfile_dir = os.path.dirname(tarfile)
  buildbot_common.Archive(tarname, bucket_path, tarfile_dir)

  # generate "manifest snippet" for this archive.
  archive_url = GSTORE + 'nacl_sdk/%s/%s' % (
      build_utils.ChromeVersion(), tarname)
  bundle = GetManifestBundle(pepper_ver, revision, tarfile, archive_url)

  manifest_snippet_file = os.path.join(OUT_DIR, tarname + '.json')
  with open(manifest_snippet_file, 'wb') as manifest_snippet_stream:
    manifest_snippet_stream.write(bundle.GetDataAsString())

  buildbot_common.Archive(tarname + '.json', bucket_path, OUT_DIR,
                          step_link=False)


def BuildStepArchiveSDKTools():
  # Only push up sdk_tools.tgz on the linux buildbot.
  builder_name = os.getenv('BUILDBOT_BUILDERNAME','')
  if builder_name == 'linux-sdk-multi':
    buildbot_common.BuildStep('Archive SDK Tools')
    bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s' % (
        build_utils.ChromeVersion(),)
    buildbot_common.Archive('sdk_tools.tgz', bucket_path, OUT_DIR,
                            step_link=False)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--pnacl', help='Enable pnacl build.',
      action='store_true', dest='pnacl', default=False)
  parser.add_option('--examples', help='Only build the examples.',
      action='store_true', dest='only_examples', default=False)
  parser.add_option('--update', help='Only build the updater.',
      action='store_true', dest='only_updater', default=False)
  parser.add_option('--test-examples',
      help='Run the pyauto tests for examples.', action='store_true',
      dest='test_examples', default=False)
  parser.add_option('--skip-tar', help='Skip generating a tarball.',
      action='store_true', dest='skip_tar', default=False)
  parser.add_option('--archive', help='Force the archive step.',
      action='store_true', dest='archive', default=False)
  parser.add_option('--gyp',
      help='Use gyp to build examples/libraries/Makefiles.',
      action='store_true')
  parser.add_option('--release', help='PPAPI release version.',
      dest='release', default=None)
  parser.add_option('--experimental',
      help='build experimental examples and libraries', action='store_true',
      dest='build_experimental', default=False)

  global options
  options, args = parser.parse_args(args[1:])
  platform = getos.GetPlatform()
  arch = 'x86'

  generate_make.use_gyp = options.gyp

  builder_name = os.getenv('BUILDBOT_BUILDERNAME','')
  if builder_name.find('pnacl') >= 0 and builder_name.find('sdk') >= 0:
    options.pnacl = True

  # TODO(binji) There is currently a hack in download_nacl_toolchains.py that
  # won't download pnacl toolchains unless the BUILDBOT_BUILDERNAME has "pnacl"
  # and "sdk" in it. Set that here, if not already set...
  if options.pnacl and not os.getenv('BUILDBOT_BUILDERNAME'):
    os.environ['BUILDBOT_BUILDERNAME'] = 'pnacl-sdk'

  # TODO(binji) for now, only test examples on non-trybots. Trybots don't build
  # pyauto Chrome.
  if buildbot_common.IsSDKBuilder():
    options.test_examples = True

  if options.pnacl:
    toolchains = ['pnacl']
  else:
    toolchains = ['newlib', 'glibc', 'host']
  print 'Building: ' + ' '.join(toolchains)

  if options.archive and (options.only_examples or options.skip_tar):
    parser.error('Incompatible arguments with archive.')

  pepper_ver = str(int(build_utils.ChromeMajorVersion()))
  pepper_old = str(int(build_utils.ChromeMajorVersion()) - 1)
  pepperdir = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_ver)
  pepperdir_old = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_old)
  clnumber = build_utils.ChromeRevision()
  tarname = 'naclsdk_' + platform + '.tar.bz2'
  if 'pnacl' in toolchains:
    tarname = 'p' + tarname
  tarfile = os.path.join(SERVER_DIR, tarname)

  if options.release:
    pepper_ver = options.release
  print 'Building PEPPER %s at %s' % (pepper_ver, clnumber)

  if options.only_examples:
    BuildStepCopyExamples(pepperdir, toolchains, options.build_experimental)
    BuildStepBuildLibraries(pepperdir, platform, 'src')
    BuildStepBuildExamples(pepperdir, platform)
    BuildStepCopyTests(pepperdir, toolchains, options.build_experimental)
    BuildStepBuildTests(pepperdir, platform)
    if options.test_examples:
      BuildStepRunPyautoTests(pepperdir, platform, pepper_ver)
  elif options.only_updater:
    build_updater.BuildUpdater(OUT_DIR)
  else:  # Build everything.
    BuildStepBuildToolsTests()

    BuildStepDownloadToolchains(platform)
    BuildStepCleanPepperDirs(pepperdir, pepperdir_old)
    BuildStepMakePepperDirs(pepperdir, ['include', 'toolchain', 'tools'])
    BuildStepCopyTextFiles(pepperdir, pepper_ver, clnumber)
    BuildStepUntarToolchains(pepperdir, platform, arch, toolchains)
    BuildStepBuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains)
    InstallHeaders(os.path.join(pepperdir, 'include'), None, 'libs')
    BuildStepCopyBuildHelpers(pepperdir, platform)
    BuildStepCopyExamples(pepperdir, toolchains, options.build_experimental)

    # Ship with libraries prebuilt, so run that first.
    BuildStepBuildLibraries(pepperdir, platform, 'src')

    if not options.skip_tar:
      BuildStepTarBundle(pepper_ver, tarfile)
      build_updater.BuildUpdater(OUT_DIR)

      # BuildStepTestUpdater downloads the bundle to its own directory. Build
      # the examples and test from this directory instead of the original.
      pepperdir = BuildStepTestUpdater(platform, pepper_ver, clnumber, tarfile)
      BuildStepBuildExamples(pepperdir, platform)
      BuildStepCopyTests(pepperdir, toolchains, options.build_experimental)
      BuildStepBuildTests(pepperdir, platform)
      if options.test_examples:
        BuildStepRunPyautoTests(pepperdir, platform, pepper_ver)

      # Archive on non-trybots.
      if options.archive or buildbot_common.IsSDKBuilder():
        BuildStepArchiveBundle(pepper_ver, clnumber, tarfile)
        BuildStepArchiveSDKTools()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
