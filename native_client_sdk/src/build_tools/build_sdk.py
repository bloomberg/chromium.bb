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
import optparse
import os
import subprocess
import sys

# local includes
import build_utils
import lastchange

# Create the various paths of interest
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')
OUT_DIR = os.path.join(SRC_DIR, 'out')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')


# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))

import getos
import http_download
import oshelpers

GSTORE = 'http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/'
MAKE = 'nacl_sdk/make_3_81/make.exe'
# For buildbots assume gsutil is stored in the build directory.
BOT_GSUTIL = '/b/build/scripts/slave/gsutil'
# For local runs just make sure gsutil is in your PATH.
LOCAL_GSUTIL = 'gsutil'
CYGTAR = os.path.join(NACL_DIR, 'build', 'cygtar.py')


def ErrorExit(msg):
  """Write and error to stderr, then exit with 1 signaling failure."""
  sys.stderr.write(msg + '\n')
  sys.exit(1)


def BuildStep(name):
  """Annotate a buildbot build step."""
  sys.stdout.flush()
  print '\n@@@BUILD_STEP %s@@@' % name
  sys.stdout.flush()


def Run(args, cwd=None, shell=False):
  """Start a process with the provided arguments.
  
  Starts a process in the provided directory given the provided arguments. If
  shell is not False, the process is launched via the shell to provide shell
  interpretation of the arguments.  Shell behavior can differ between platforms
  so this should be avoided when not using platform dependent shell scripts."""
  print 'Running: ' + ' '.join(args)
  sys.stdout.flush()
  sys.stderr.flush()
  subprocess.check_call(args, cwd=cwd, shell=shell)
  sys.stdout.flush()
  sys.stderr.flush()


def Archive(filename):
  """Upload the given filename to Google Store."""
  chrome_version = build_utils.ChromeVersion()
  bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s/%s' % (
      chrome_version, filename)
  full_dst = 'gs://%s' % bucket_path

  if os.environ.get('BUILDBOT_BUILDERNAME', ''):
    gsutil = BOT_GSUTIL
  else:
    gsutil = LOCAL_GSUTIL

  subprocess.check_call(
      '%s cp -a public-read %s %s' % (
          gsutil, filename, full_dst), shell=True, cwd=OUT_DIR)
  url = 'https://commondatastorage.googleapis.com/%s' % bucket_path
  print '@@@STEP_LINK@download@%s@@@' % url
  sys.stdout.flush()


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.
  
  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""
  
  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    ErrorExit('Make.bat not relative to Pepper directory: ' + makepath)
  
  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


def CopyDir(src, dst, excludes=['.svn','*/.svn']):
  """Recursively copy a directory using."""
  args = ['-r', src, dst]
  for exc in excludes:
    args.append('--exclude=' + exc)
  print "cp -r %s %s" % (src, dst)
  oshelpers.Copy(args)


def RemoveDir(dst):
  """Remove the provided path."""
  print "rm -fr " + dst
  oshelpers.Remove(['-fr', dst])


def MakeDir(dst):
  """Create the path including all parent directories as needed."""
  print "mkdir -p " + dst
  oshelpers.Mkdir(['-p', dst])


def MoveDir(src, dst):
  """Move the path src to dst."""
  print "mv -fr %s %s" % (src, dst)
  oshelpers.Move(['-f', src, dst])


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


def GetPNaClToolchain(platform, arch):
  tcdir = os.path.join(NACL_DIR, 'toolchain', '.tars')
  if arch == 'x86':
    tcname = 'naclsdk_pnacl_%s_%s_64.tgz' % (platform, arch)
  else:
    ErrorExit('Unknown architecture.')
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
    ErrorExit('Unknown architecture.')


def GetToolchainNaClLib(tcname, tcpath, arch, xarch):
  if arch == 'x86':
    if tcname == 'pnacl':
      return os.path.join(tcpath, 'newlib', 'sdk', 'lib')
    if str(xarch) == '32':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
    if str(xarch) == '64':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib')
  ErrorExit('Unknown architecture.')


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
    MakeDir(os.path.dirname(dst))
    oshelpers.Copy(['-v', src, dst])

  # Clean out per toolchain ppapi directory
  ppapi = os.path.join(tc_dst_inc, 'ppapi')
  RemoveDir(ppapi)

  # Copy in c and c/dev headers
  MakeDir(os.path.join(ppapi, 'c', 'dev'))
  CopyDir(os.path.join(PPAPI_DIR, 'c', '*.h'),
          os.path.join(ppapi, 'c'))
  CopyDir(os.path.join(PPAPI_DIR, 'c', 'dev', '*.h'),
          os.path.join(ppapi, 'c', 'dev'))

  # Run the generator to overwrite IDL files  
  Run([sys.executable, 'generator.py', '--wnone', '--cgen',
       '--release=M' + pepper_ver, '--verbose', '--dstroot=%s/c' % ppapi],
       cwd=os.path.join(PPAPI_DIR, 'generators'))

  # Remove private and trusted interfaces
  RemoveDir(os.path.join(ppapi, 'c', 'private'))
  RemoveDir(os.path.join(ppapi, 'c', 'trusted'))

  # Copy in the C++ headers
  MakeDir(os.path.join(ppapi, 'cpp', 'dev'))
  CopyDir(os.path.join(PPAPI_DIR, 'cpp','*.h'),
          os.path.join(ppapi, 'cpp'))
  CopyDir(os.path.join(PPAPI_DIR, 'cpp', 'dev', '*.h'),
          os.path.join(ppapi, 'cpp', 'dev'))
  MakeDir(os.path.join(ppapi, 'utility', 'graphics'))
  CopyDir(os.path.join(PPAPI_DIR, 'utility','*.h'),
          os.path.join(ppapi, 'utility'))
  CopyDir(os.path.join(PPAPI_DIR, 'utility', 'graphics', '*.h'),
          os.path.join(ppapi, 'utility', 'graphics'))

  # Copy in the gles2 headers
  MakeDir(os.path.join(ppapi, 'gles2'))
  CopyDir(os.path.join(PPAPI_DIR,'lib','gl','gles2','*.h'),
          os.path.join(ppapi, 'gles2'))

  # Copy the EGL headers
  MakeDir(os.path.join(tc_dst_inc, 'EGL'))
  CopyDir(os.path.join(PPAPI_DIR,'lib','gl','include','EGL', '*.h'),
          os.path.join(tc_dst_inc, 'EGL'))

  # Copy the GLES2 headers
  MakeDir(os.path.join(tc_dst_inc, 'GLES2'))
  CopyDir(os.path.join(PPAPI_DIR,'lib','gl','include','GLES2', '*.h'),
          os.path.join(tc_dst_inc, 'GLES2'))

  # Copy the KHR headers
  MakeDir(os.path.join(tc_dst_inc, 'KHR'))
  CopyDir(os.path.join(PPAPI_DIR,'lib','gl','include','KHR', '*.h'),
          os.path.join(tc_dst_inc, 'KHR'))


def UntarToolchains(pepperdir, platform, arch, toolchains):
  BuildStep('Untar Toolchains')
  tcname = platform + '_' + arch
  tmpdir = os.path.join(SRC_DIR, 'out', 'tc_temp')
  RemoveDir(tmpdir)
  MakeDir(tmpdir)

  if 'newlib' in toolchains:
    # Untar the newlib toolchains
    tarfile = GetNewlibToolchain(platform, arch)
    Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile], cwd=NACL_DIR)
  
    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'sdk', 'nacl-sdk')
    newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
    MoveDir(srcdir, newlibdir)

  if 'glibc' in toolchains:
    # Untar the glibc toolchains
    tarfile = GetGlibcToolchain(platform, arch)
    Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile], cwd=NACL_DIR)
  
    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'toolchain', tcname)
    glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
    MoveDir(srcdir, glibcdir)

  # Untar the pnacl toolchains
  if 'pnacl' in toolchains:
    tmpdir = os.path.join(tmpdir, 'pnacl')
    RemoveDir(tmpdir)
    MakeDir(tmpdir)
    tarfile = GetPNaClToolchain(platform, arch)
    Run([sys.executable, CYGTAR, '-C', tmpdir, '-xf', tarfile], cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    pnacldir = os.path.join(pepperdir, 'toolchain', tcname + '_pnacl')
    MoveDir(tmpdir, pnacldir)


def BuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains):
  BuildStep('SDK Items')

  tcname = platform + '_' + arch
  newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
  glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
  pnacldir = os.path.join(pepperdir, 'toolchain', tcname + '_pnacl')

  # Run scons TC build steps
  if arch == 'x86':
    if 'newlib' in toolchains:
      Run(GetBuildArgs('newlib', newlibdir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      Run(GetBuildArgs('newlib', newlibdir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      InstallHeaders(GetToolchainNaClInclude('newlib', newlibdir, 'x86'),
                     pepper_ver, 
                     'newlib')

    if 'glibc' in toolchains:
      Run(GetBuildArgs('glibc', glibcdir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      Run(GetBuildArgs('glibc', glibcdir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      InstallHeaders(GetToolchainNaClInclude('glibc', glibcdir, 'x86'),
                     pepper_ver,
                     'glibc')

    if 'pnacl' in toolchains:
      Run(GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '32'),
          cwd=NACL_DIR, shell=(platform=='win'))
      Run(GetBuildArgs('pnacl', pnacldir, pepperdir, 'x86', '64'),
          cwd=NACL_DIR, shell=(platform=='win'))
      InstallHeaders(GetToolchainNaClInclude('pnacl', pnacldir, 'x86'),
                     pepper_ver, 
                     'newlib')
  else:
    ErrorExit('Missing arch %s' % arch)


EXAMPLE_MAP = {
  'newlib': [
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

def CopyExamples(pepperdir, toolchains):
  BuildStep('Copy examples')

  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    ErrorExit('Examples depend on missing toolchains.')

  exampledir = os.path.join(pepperdir, 'examples')
  RemoveDir(exampledir)
  MakeDir(exampledir)
  AddMakeBat(pepperdir, exampledir)

  # Copy individual files
  files = ['favicon.ico', 'httpd.cmd', 'httpd.py', 'index.html', 'Makefile']
  for filename in files:
    oshelpers.Copy(['-v', os.path.join(SDK_EXAMPLE_DIR, filename), exampledir])

  # Add examples for supported toolchains
  examples = []
  for tc in toolchains:
    examples.extend(EXAMPLE_MAP[tc])
  for example in examples:
    CopyDir(os.path.join(SDK_EXAMPLE_DIR, example), exampledir)
    AddMakeBat(pepperdir, os.path.join(exampledir, example))


def BuildUpdater():
  BuildStep('Create Updater')
  tooldir = os.path.join(SRC_DIR, 'out', 'sdk_tools')
  sdkupdate = os.path.join(SDK_SRC_DIR, 'build_tools', 'sdk_tools', 'sdk_update.py')
  license = os.path.join(SDK_SRC_DIR, 'LICENSE')
  RemoveDir(tooldir)
  MakeDir(tooldir)
  args = ['-v', sdkupdate, license, CYGTAR, tooldir]
  oshelpers.Copy(args)
  tarname = 'sdk_tools.tgz'
  tarfile = os.path.join(OUT_DIR, tarname)
  Run([sys.executable, CYGTAR, '-C', tooldir, '-czf', tarfile,
       'sdk_update.py', 'LICENSE', 'cygtar.py'], cwd=NACL_DIR)
  sys.stdout.write('\n')


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--pnacl', help='Enable pnacl build.',
      action='store_true', dest='pnacl', default=False)
  parser.add_option('--examples', help='Rebuild the examples.',
      action='store_true', dest='examples', default=False)
  parser.add_option('--update', help='Rebuild the updater.',
      action='store_true', dest='update', default=False)
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
  skip = options.examples or options.update

  skip_examples = skip
  skip_update = skip
  skip_untar = skip
  skip_build = skip
  skip_tar = skip or options.skip_tar


  if options.examples: skip_examples = False
  if options.update: skip_update = False

  if options.archive and (options.examples or options.skip_tar):
    parser.error('Incompatible arguments with archive.')

  # TODO(noelallen): Remove force build to 18...
  pepper_ver = str(int(build_utils.ChromeMajorVersion()) - 1)
  clnumber = lastchange.FetchVersionInfo(None).revision
  if options.release:
    pepper_ver = options.release
  print 'Building PEPPER %s at %s' % (pepper_ver, clnumber)

  if not skip_build:
    BuildStep('Rerun hooks to get toolchains')
    Run(['gclient', 'runhooks'], cwd=SRC_DIR, shell=(platform=='win'))

  BuildStep('Clean Pepper Dir')
  pepperdir = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_ver)
  if not skip_untar:
    RemoveDir(pepperdir)
    MakeDir(os.path.join(pepperdir, 'toolchain'))
    MakeDir(os.path.join(pepperdir, 'tools'))

  BuildStep('Add Text Files')
  files = ['AUTHORS', 'COPYING', 'LICENSE', 'NOTICE', 'README']
  files = [os.path.join(SDK_SRC_DIR, filename) for filename in files]
  oshelpers.Copy(['-v'] + files + [pepperdir])


  # Clean out the temporary toolchain untar directory
  if not skip_untar:
    UntarToolchains(pepperdir, platform, arch, toolchains)

  if not skip_build:
    BuildToolchains(pepperdir, platform, arch, pepper_ver, toolchains)

  BuildStep('Copy make OS helpers')
  CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
      os.path.join(pepperdir, 'tools'))
  if platform == 'win':
    BuildStep('Add MAKE')
    http_download.HttpDownload(GSTORE + MAKE,
                             os.path.join(pepperdir, 'tools' ,'make.exe'))

  if not skip_examples:
    CopyExamples(pepperdir, toolchains)

  if not skip_tar:
    BuildStep('Tar Pepper Bundle')
    tarname = 'naclsdk_' + platform + '.bz2'
    if 'pnacl' in toolchains:
      tarname = 'p' + tarname
    tarfile = os.path.join(OUT_DIR, tarname)
    Run([sys.executable, CYGTAR, '-C', OUT_DIR, '-cjf', tarfile,
         'pepper_' + pepper_ver], cwd=NACL_DIR)

  # Archive on non-trybots.
  if options.archive or '-sdk' in os.environ.get('BUILDBOT_BUILDERNAME', ''):
    BuildStep('Archive build')
    Archive(tarname)

  if not skip_examples:
    BuildStep('Test Build Examples')
    filelist = os.listdir(os.path.join(pepperdir, 'examples'))
    for filenode in filelist:
      dirnode = os.path.join(pepperdir, 'examples', filenode)
      makefile = os.path.join(dirnode, 'Makefile')
      if os.path.isfile(makefile):
        print "\n\nMake: " + dirnode
        Run(['make', 'all', '-j8'], cwd=os.path.abspath(dirnode), shell=True)

# Build SDK Tools
#  if not skip_update:
#    BuildUpdater()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
