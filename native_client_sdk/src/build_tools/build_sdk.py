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


# std python includes
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

GSTORE = 'http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/'
MAKE = 'nacl_sdk/make_3_81/make.exe'
CYGTAR = os.path.join(NACL_DIR, 'build', 'cygtar.py')


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
  bin = 'bindir=' + os.path.join(outdir, 'tools')
  lib = 'libdir=' + GetToolchainNaClLib(tcname, tcpath, arch, xarch)
  args = [scons, mode, plat, bin, lib, '-j10',
          'install_bin', 'install_lib']
  if tcname == 'glibc':
    args.append('--nacl_glibc')

  if tcname == 'pnacl':
    args.append('bitcode=1')

  print "Building %s (%s): %s" % (tcname, arch, ' '.join(args))
  return args


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
  buildbot_common.Run([sys.executable, 'generator.py', '--wnone', '--cgen',
       '--release=M' + pepper_ver, '--verbose', '--dstroot=%s/c' % ppapi],
       cwd=os.path.join(PPAPI_DIR, 'generators'))

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
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'utility','*.h'),
          os.path.join(ppapi, 'utility'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR, 'utility', 'graphics', '*.h'),
          os.path.join(ppapi, 'utility', 'graphics'))

  # Copy in the gles2 headers
  buildbot_common.MakeDir(os.path.join(ppapi, 'gles2'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR,'lib','gl','gles2','*.h'),
          os.path.join(ppapi, 'gles2'))

  # Copy the EGL headers
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'EGL'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR,'lib','gl','include','EGL', '*.h'),
          os.path.join(tc_dst_inc, 'EGL'))

  # Copy the GLES2 headers
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'GLES2'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR,'lib','gl','include','GLES2', '*.h'),
          os.path.join(tc_dst_inc, 'GLES2'))

  # Copy the KHR headers
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'KHR'))
  buildbot_common.CopyDir(
          os.path.join(PPAPI_DIR,'lib','gl','include','KHR', '*.h'),
          os.path.join(tc_dst_inc, 'KHR'))

  # Copy the lib files
  buildbot_common.MakeDir(os.path.join(tc_dst_inc, 'lib'))
  buildbot_common.CopyDir(os.path.join(PPAPI_DIR,'lib'), tc_dst_inc)


def UntarToolchains(pepperdir, platform, arch, toolchains):
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


def BuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains):
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


EXAMPLE_MAP = {
  'newlib': [
    'debugging',
    'file_histogram',
    'file_io',
    'fullscreen_tumbler',
    'gamepad',
    'geturl',
    'hello_world_interactive',
    'hello_world',
#    'hello_world_gles',
    'input_events',
    'load_progress',
    'mouselock',
    'multithreaded_input_events',
    'pi_generator',
    'pong',
    'sine_synth',
    'tumbler',
    'websocket'
  ],
  'glibc': [
    'dlopen',
  ],
  'pnacl': [
#    'hello_world_pnacl',
  ],
}


LIBRARY_MAP = {
  'newlib': [
    'gles2',
  ],
  'glibc': [],
  'pnacl': []
}


def CopyExamples(pepperdir, toolchains):
  buildbot_common.BuildStep('Copy examples')

  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    buildbot_common.ErrorExit('Examples depend on missing toolchains.')

  exampledir = os.path.join(pepperdir, 'examples')
  buildbot_common.RemoveDir(exampledir)
  buildbot_common.MakeDir(exampledir)

  # Copy individual files
  files = ['favicon.ico', 'httpd.cmd', 'httpd.py', 'index.html']
  for filename in files:
    oshelpers.Copy(['-v', os.path.join(SDK_EXAMPLE_DIR, filename), exampledir])

  # Add examples for supported toolchains
  examples = []
  for tc in toolchains:
    examples.extend(EXAMPLE_MAP[tc])

  libraries = []
  for tc in toolchains:
    libraries.extend(LIBRARY_MAP[tc])

  print 'Process: ' + ' '.join(examples)
  print 'Process: ' + ' '.join(libraries)
  args = ['--dstroot=%s' % pepperdir, '--master']
  for example in examples:
    dsc = os.path.join(SDK_EXAMPLE_DIR, example, 'example.dsc')
    args.append(dsc)
  for library in libraries:
    dsc = os.path.join(SDK_LIBRARY_DIR, library, 'library.dsc')
    args.append(dsc)

  if generate_make.main(args):
    buildbot_common.ErrorExit('Failed to build examples.')


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--pnacl', help='Enable pnacl build.',
      action='store_true', dest='pnacl', default=False)
  parser.add_option('--examples', help='Only build the examples.',
      action='store_true', dest='only_examples', default=False)
  parser.add_option('--update', help='Only build the updater.',
      action='store_true', dest='only_updater', default=False)
  parser.add_option('--skip-tar', help='Skip generating a tarball.',
      action='store_true', dest='skip_tar', default=False)
  parser.add_option('--archive', help='Force the archive step.',
      action='store_true', dest='archive', default=False)
  parser.add_option('--release', help='PPAPI release version.',
      dest='release', default=None)

  options, args = parser.parse_args(args[1:])
  platform = getos.GetPlatform()
  arch = 'x86'

  builder_name = os.getenv('BUILDBOT_BUILDERNAME','')
  if builder_name.find('pnacl') >= 0 and builder_name.find('sdk') >= 0:
    options.pnacl = True

  if options.pnacl:
    toolchains = ['pnacl']
  else:
    toolchains = ['newlib', 'glibc']
  print 'Building: ' + ' '.join(toolchains)
  skip = options.only_examples or options.only_updater

  skip_examples = skip and not options.only_examples
  skip_update = skip and not options.only_updater
  skip_untar = skip
  skip_build = skip
  skip_test_updater = skip
  skip_tar = skip or options.skip_tar

  if options.archive and (options.only_examples or options.skip_tar):
    parser.error('Incompatible arguments with archive.')

  pepper_ver = str(int(build_utils.ChromeMajorVersion()))
  pepper_old = str(int(build_utils.ChromeMajorVersion()) - 1)
  clnumber = build_utils.ChromeRevision()
  if options.release:
    pepper_ver = options.release
  print 'Building PEPPER %s at %s' % (pepper_ver, clnumber)

  if not skip_build:
    buildbot_common.BuildStep('Rerun hooks to get toolchains')
    buildbot_common.Run(['gclient', 'runhooks'],
                        cwd=SRC_DIR, shell=(platform=='win'))

  buildbot_common.BuildStep('Clean Pepper Dirs')
  pepperdir = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_ver)
  pepperold = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_old)
  buildbot_common.RemoveDir(pepperold)
  if not skip_untar:
    buildbot_common.RemoveDir(pepperdir)
    buildbot_common.MakeDir(os.path.join(pepperdir, 'src'))
    buildbot_common.MakeDir(os.path.join(pepperdir, 'toolchain'))
    buildbot_common.MakeDir(os.path.join(pepperdir, 'tools'))
  else:
    buildbot_common.MakeDir(pepperdir)

  if not skip_build:
    buildbot_common.BuildStep('Add Text Files')
    files = ['AUTHORS', 'COPYING', 'LICENSE', 'NOTICE', 'README']
    files = [os.path.join(SDK_SRC_DIR, filename) for filename in files]
    oshelpers.Copy(['-v'] + files + [pepperdir])


  # Clean out the temporary toolchain untar directory
  if not skip_untar:
    UntarToolchains(pepperdir, platform, arch, toolchains)

  if not skip_build:
    BuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains)
    InstallHeaders(os.path.join(pepperdir, 'src'), pepper_ver, 'libs')

  if not skip_build:
    buildbot_common.BuildStep('Copy make OS helpers')
    buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
        os.path.join(pepperdir, 'tools'))
    if platform == 'win':
      buildbot_common.BuildStep('Add MAKE')
      http_download.HttpDownload(GSTORE + MAKE,
                                 os.path.join(pepperdir, 'tools' ,'make.exe'))
      rename_list = ['ncval_x86_32', 'ncval_x86_64',
                     'sel_ldr_x86_32', 'sel_ldr_x86_64']
      tools = os.path.join(pepperdir, 'tools')
      for name in rename_list:
          src = os.path.join(pepperdir, 'tools', name)
          dst = os.path.join(pepperdir, 'tools', name + '.exe')
          buildbot_common.Move(src, dst)

  if not skip_examples:
    CopyExamples(pepperdir, toolchains)

  tarname = 'naclsdk_' + platform + '.bz2'
  if 'pnacl' in toolchains:
    tarname = 'p' + tarname
  tarfile = os.path.join(OUT_DIR, tarname)

  if not skip_tar:
    buildbot_common.BuildStep('Tar Pepper Bundle')
    buildbot_common.Run([sys.executable, CYGTAR, '-C', OUT_DIR, '-cjf', tarfile,
         'pepper_' + pepper_ver], cwd=NACL_DIR)

  # Run build tests
  buildbot_common.BuildStep('Run build_tools tests')
  buildbot_common.Run([sys.executable,
      os.path.join(SDK_SRC_DIR, 'build_tools', 'tests', 'test_all.py')])

  # build sdk update
  if not skip_update:
    build_updater.BuildUpdater(OUT_DIR)

  # start local server sharing a manifest + the new bundle
  if not skip_test_updater and not skip_tar:
    buildbot_common.BuildStep('Move bundle to localserver dir')
    buildbot_common.MakeDir(SERVER_DIR)
    buildbot_common.Move(tarfile, SERVER_DIR)
    tarfile = os.path.join(SERVER_DIR, tarname)

    server = None
    try:
      buildbot_common.BuildStep('Run local server')
      server = test_server.LocalHTTPServer(SERVER_DIR)

      buildbot_common.BuildStep('Generate manifest')
      with open(tarfile, 'rb') as tarfile_stream:
        archive_sha1, archive_size = manifest_util.DownloadAndComputeHash(
            tarfile_stream)
      archive = manifest_util.Archive(manifest_util.GetHostOS())
      archive.CopyFrom({'url': server.GetURL(tarname),
                        'size': archive_size,
                        'checksum': {'sha1': archive_sha1}})
      bundle = manifest_util.Bundle('pepper_' + pepper_ver)
      bundle.CopyFrom({
          'revision': int(clnumber),
          'repath': 'pepper_' + pepper_ver,
          'version': int(pepper_ver),
          'description': 'Chrome %s bundle, revision %s' % (
              pepper_ver, clnumber),
          'stability': 'dev',
          'recommended': 'no',
          'archives': [archive]})
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

      # If we are testing examples, do it in the newly pulled directory.
      pepperdir = os.path.join(OUT_DIR, 'nacl_sdk', 'pepper_' + pepper_ver)

    # kill server
    finally:
      if server:
        server.Shutdown()

  # build examples.
  if not skip_examples:
    buildbot_common.BuildStep('Test Build Examples')
    example_dir = os.path.join(pepperdir, 'examples')
    makefile = os.path.join(example_dir, 'Makefile')
    if os.path.isfile(makefile):
      print "\n\nMake: " + example_dir
      buildbot_common.Run(['make', '-j8'],
                          cwd=os.path.abspath(example_dir), shell=True)

  # test examples.
  skip_test_examples = True
  if not skip_examples:
    if not skip_test_examples:
      run_script_path = os.path.join(SRC_DIR, 'chrome', 'test', 'functional')
      buildbot_common.Run([sys.executable, 'nacl_sdk_example_test.py',
        'nacl_sdk_example_test.NaClSDKTest.testNaClSDK'], cwd=run_script_path,
        env=dict(os.environ.items()+{'pepper_ver':pepper_ver,
        'OUT_DIR':OUT_DIR}.items()))

  # Archive on non-trybots.
  buildername = os.environ.get('BUILDBOT_BUILDERNAME', '')
  if options.archive or '-sdk' in buildername:
    buildbot_common.BuildStep('Archive build')
    bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s' % \
        build_utils.ChromeVersion()
    buildbot_common.Archive(tarname, bucket_path, os.path.dirname(tarfile))

    if not skip_update:
      # Only push up sdk_tools.tgz on the linux buildbot.
      if buildername == 'linux-sdk-multi':
        sdk_tools = os.path.join(OUT_DIR, 'sdk_tools.tgz')
        buildbot_common.Archive('sdk_tools.tgz', bucket_path, OUT_DIR,
                                step_link=False)

    # generate "manifest snippet" for this archive.
    if not skip_test_updater:
      archive = bundle.GetArchive(manifest_util.GetHostOS())
      archive.url = 'https://commondatastorage.googleapis.com/' \
          'nativeclient-mirror/nacl/nacl_sdk/%s/%s' % (
              build_utils.ChromeVersion(), tarname)
      manifest_snippet_file = os.path.join(OUT_DIR, tarname + '.json')
      with open(manifest_snippet_file, 'wb') as manifest_snippet_stream:
        manifest_snippet_stream.write(bundle.GetDataAsString())

      buildbot_common.Archive(tarname + '.json', bucket_path, OUT_DIR,
                              step_link=False)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
