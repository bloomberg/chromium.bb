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
import multiprocessing
import optparse
import os
import platform
import subprocess
import sys

# local includes
import buildbot_common
import build_utils
import lastchange
import manifest_util

# Create the various paths of interest
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
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


def HTTPServerProcess(conn, serve_dir):
  """Run a local httpserver with a randomly-chosen port.

  This function assumes it is run as a child process using multiprocessing.

  Args:
    conn: A connection to the parent process. The child process sends
        the local port, and waits for a message from the parent to
        stop serving.
    serve_dir: The directory to serve. All files are accessible through
       http://localhost:<port>/path/to/filename.
  """
  import BaseHTTPServer
  import SimpleHTTPServer

  os.chdir(serve_dir)
  httpd = BaseHTTPServer.HTTPServer(('', 0),
                                    SimpleHTTPServer.SimpleHTTPRequestHandler)
  conn.send(httpd.server_address[1])  # the chosen port number
  httpd.timeout = 0.5  # seconds
  running = True
  while running:
    httpd.handle_request()
    if conn.poll():
      running = conn.recv()
  conn.close()


class LocalHTTPServer(object):
  """Class to start a local HTTP server as a child process."""

  def __init__(self, serve_dir):
    parent_conn, child_conn = multiprocessing.Pipe()
    self.process = multiprocessing.Process(target=HTTPServerProcess,
                                           args=(child_conn, serve_dir))
    self.process.start()
    if parent_conn.poll(10):  # wait 10 seconds
      self.port = parent_conn.recv()
    else:
      raise Exception('Unable to launch HTTP server.')

    self.conn = parent_conn

  def Shutdown(self):
    """Send a message to the child HTTP server process and wait for it to
        finish."""
    self.conn.send(False)
    self.process.join()

  def GetURL(self, rel_url):
    """Get the full url for a file on the local HTTP server.

    Args:
      rel_url: A URL fragment to convert to a full URL. For example,
          GetURL('foobar.baz') -> 'http://localhost:1234/foobar.baz'
    """
    return 'http://localhost:%d/%s' % (self.port, rel_url)


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.

  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""

  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    buildbot_common.ErrorExit('Make.bat not relative to Pepper directory: ' +
                              makepath)

  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


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
    'fullscreen_tumbler',
    'gamepad',
    'geturl',
    'hello_world_interactive',
    'hello_world_newlib',
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
    'hello_world_glibc',
  ],
  'pnacl': [
    'hello_world_pnacl',
  ],
}

def GenerateExamplesMakefile(in_path, out_path, examples):
  """Generate a Makefile that includes only the examples supported by this
  SDK."""
  # Line wrap the PROJECTS variable
  wrap_width = 80
  projects_text = ''
  projects_line = 'PROJECTS:='
  for example in examples:
    if len(projects_line + example + ' ') > wrap_width:
      projects_text += projects_line + '\n'
      projects_line = 'PROJECTS+='
    projects_line += example + ' '

  # Add the last unwrapped line
  projects_text += projects_line + '\n'

  out_makefile_text = ''
  wrote_projects_text = False
  snipping = False
  for line in open(in_path, 'r'):
    if line.startswith('# =SNIP='):
      snipping = not snipping
      continue

    if snipping:
      if not wrote_projects_text:
        out_makefile_text += projects_text
      wrote_projects_text = True
    else:
      out_makefile_text += line

  open(out_path, 'w').write(out_makefile_text)


def CopyExamples(pepperdir, toolchains):
  buildbot_common.BuildStep('Copy examples')

  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    buildbot_common.ErrorExit('Examples depend on missing toolchains.')

  exampledir = os.path.join(pepperdir, 'examples')
  buildbot_common.RemoveDir(exampledir)
  buildbot_common.MakeDir(exampledir)
  AddMakeBat(pepperdir, exampledir)

  # Copy individual files
  files = ['favicon.ico', 'httpd.cmd', 'httpd.py', 'index.html']
  for filename in files:
    oshelpers.Copy(['-v', os.path.join(SDK_EXAMPLE_DIR, filename), exampledir])

  # Add examples for supported toolchains
  examples = []
  for tc in toolchains:
    examples.extend(EXAMPLE_MAP[tc])
  for example in examples:
    buildbot_common.CopyDir(os.path.join(SDK_EXAMPLE_DIR, example), exampledir)
    AddMakeBat(pepperdir, os.path.join(exampledir, example))

  # Generate a root Makefile that only includes supported examples
  out_makefile = os.path.join(exampledir, 'Makefile')
  print 'Generating %s' % (out_makefile,)
  GenerateExamplesMakefile(os.path.join(SDK_EXAMPLE_DIR, 'Makefile'),
      out_makefile, examples)

UPDATER_FILES = [
  # launch scripts
  ('build_tools/naclsdk', 'nacl_sdk/naclsdk'),
  ('build_tools/naclsdk.bat', 'nacl_sdk/naclsdk.bat'),

  # base manifest
  ('build_tools/json/naclsdk_manifest0.json',
      'nacl_sdk/sdk_cache/naclsdk_manifest2.json'),

  # SDK tools
  ('build_tools/sdk_tools/cacerts.txt', 'nacl_sdk/sdk_tools/cacerts.txt'),
  ('build_tools/sdk_tools/sdk_update.py', 'nacl_sdk/sdk_tools/sdk_update.py'),
  ('build_tools/manifest_util.py', 'nacl_sdk/sdk_tools/manifest_util.py'),
  ('build_tools/sdk_tools/third_party/__init__.py',
      'nacl_sdk/sdk_tools/third_party/__init__.py'),
  ('build_tools/sdk_tools/third_party/fancy_urllib/__init__.py',
      'nacl_sdk/sdk_tools/third_party/fancy_urllib/__init__.py'),
  ('build_tools/sdk_tools/third_party/fancy_urllib/README',
      'nacl_sdk/sdk_tools/third_party/fancy_urllib/README'),
  ('LICENSE', 'nacl_sdk/sdk_tools/LICENSE'),
  (CYGTAR, 'nacl_sdk/sdk_tools/cygtar.py'),
]

def CopyFiles(files):
  for in_file, out_file in files:
    if not os.path.isabs(in_file):
      in_file = os.path.join(SDK_SRC_DIR, in_file)
    out_file = os.path.join(OUT_DIR, out_file)
    buildbot_common.MakeDir(os.path.dirname(out_file))
    buildbot_common.CopyFile(in_file, out_file)

def BuildUpdater():
  buildbot_common.BuildStep('Create Updater')

  # Build SDK directory
  buildbot_common.RemoveDir(os.path.join(OUT_DIR, 'nacl_sdk'))

  CopyFiles(UPDATER_FILES)

  # Make zip
  buildbot_common.RemoveFile(os.path.join(OUT_DIR, 'nacl_sdk.zip'))
  buildbot_common.Run([sys.executable, oshelpers.__file__, 'zip',
                       'nacl_sdk.zip'] +
                      [out_file for in_file, out_file in UPDATER_FILES],
                      cwd=OUT_DIR)

  # Tar of all files under nacl_sdk/sdk_tools
  sdktoolsdir = 'nacl_sdk/sdk_tools'
  tarname = os.path.join(OUT_DIR, 'sdk_tools.tgz')
  files_to_tar = [os.path.relpath(out_file, sdktoolsdir) for in_file, out_file
                  in UPDATER_FILES if out_file.startswith(sdktoolsdir)]
  buildbot_common.RemoveFile(tarname)
  buildbot_common.Run([sys.executable, CYGTAR, '-C',
      os.path.join(OUT_DIR, sdktoolsdir), '-czf', tarname] + files_to_tar,
      cwd=NACL_DIR)
  sys.stdout.write('\n')


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
  clnumber = lastchange.FetchVersionInfo(None).revision
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

  # build sdk update
  if not skip_update:
    BuildUpdater()

  # start local server sharing a manifest + the new bundle
  if not skip_test_updater:
    buildbot_common.BuildStep('Move bundle to localserver dir')
    buildbot_common.MakeDir(SERVER_DIR)
    buildbot_common.Move(tarfile, SERVER_DIR)
    tarfile = os.path.join(SERVER_DIR, tarname)

    server = None
    try:
      buildbot_common.BuildStep('Run local server')
      server = LocalHTTPServer(SERVER_DIR)

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
          'revision': clnumber,
          'repath': 'pepper_' + pepper_ver,
          'version': pepper_ver,
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
      updater_py = os.path.join(OUT_DIR, 'nacl_sdk', 'sdk_tools',
          'sdk_update.py')
      buildbot_common.Run([sys.executable, updater_py, '-U',
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
