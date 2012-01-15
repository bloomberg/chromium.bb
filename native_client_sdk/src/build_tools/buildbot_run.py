#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Entry point for both build and try bots'''

import build_utils
import lastchange
import os
import subprocess
import sys

# Create the various paths of interest
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')
OUT_DIR = os.path.join(SRC_DIR, 'out')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')


# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))


import http_download
from getos import GetPlatform
import oshelpers

GSTORE = 'http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/'
MAKE = 'nacl_sdk/make_3_81/make.exe'
GSUTIL = '/b/build/scripts/slave/gsutil'

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
  subprocess.check_call(args, cwd=cwd, shell=shell)
  sys.stdout.flush()


def Archive(filename):
  """Upload the given filename to Google Store."""
  chrome_version = build_utils.ChromeVersion()
  bucket_path = 'nativeclient-mirror/nacl/nacl_sdk/%s/%s' % (
      chrome_version, filename)
  full_dst = 'gs://%s' % bucket_path

  if os.environ.get('BUILDBOT_BUILDERNAME', ''):
    # For buildbots assume gsutil is stored in the build directory.
    gsutil = '/b/build/scripts/slave/gsutil'
  else:
    # For non buildpots, you must have it in your path.
    gsutil = 'gsutil'

  subprocess.check_call(
      '%s cp -a public-read %s %s' % (
          gsutil, filename, full_dst), shell=True, cwd=OUT_DIR)
  url = 'https://commondatastorage.googleapis.com/%s' % bucket_path
  print '@@@STEP_LINK@download@%s@@@' % url
  sys.stdout.flush()


def AddMakeBat(makepath):
  """Create a simple batch file to execute Make.
  
  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK.""" 
  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  fp.write('@..\\..\\tools\\make.exe %*\n')
  fp.close()


def CopyDir(src, dst, excludes=['.svn']):
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


def GetScons():
  if sys.platform in ['cygwin', 'win32']:
    return 'scons.bat'
  return './scons'


def GetArchName(arch, xarch=None):
  if xarch:
    return arch + '-' + str(xarch)
  return arch


def GetToolchainNaClInclude(tcpath, arch, xarch=None):
  if arch == 'x86':
    return os.path.join(tcpath, 'x86_64-nacl', 'include')
  else:
    ErrorExit('Unknown architecture.')


def GetToolchainNaClLib(tcpath, arch, xarch):
  if arch == 'x86':
    if str(xarch) == '32':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
    if str(xarch) == '64':
      return os.path.join(tcpath, 'x86_64-nacl', 'lib')
  ErrorExit('Unknown architecture.')


def GetBuildArgs(tcname, tcpath, arch, xarch=None):
  """Return list of scons build arguments to generate user libraries."""
  scons = GetScons()
  mode = '--mode=opt-host,nacl'
  arch_name = GetArchName(arch, xarch)
  plat = 'platform=' + arch_name
  bin = ('bindir=' +
         BuildOutputDir('pepper_' + build_utils.ChromeMajorVersion(), 'tools'))
  lib = ('libdir=' +
         os.path.join(GetToolchainNaClLib(tcpath, arch, xarch)))
  args = [scons, mode, plat, bin, lib, '-j10',
          'install_bin', 'install_lib']
  if tcname == 'glibc':
    args.append('--nacl_glibc')
  return args

header_map = {
  'newlib': {
      'pthread.h': 'src/untrusted/pthread/pthread.h',
      'semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'dynamic_annotations.h': 'src/untrusted/valgrind/dynamic_annotations.h',
      'dynamic_annotations.h': 'src/untrusted/valgrind/dynamic_annotations.h',
      'nacl_dyncode.h': ' src/untrusted/nacl/nacl_dyncode.h',
      'nacl_startup.h': 'src/untrusted/nacl/nacl_startup.h',
      'nacl_thread.h': 'src/untrusted/nacl/nacl_thread.h',
      'pnacl.h': 'src/untrusted/nacl/pnacl.h',
      'irt.h': 'src/untrusted/irt/irt.h',
      'irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
  'glibc': {
      'dynamic_annotations.h': 'src/untrusted/valgrind/dynamic_annotations.h',
      'dynamic_annotations.h': 'src/untrusted/valgrind/dynamic_annotations.h',
      'nacl_dyncode.h': ' src/untrusted/nacl/nacl_dyncode.h',
      'nacl_startup.h': 'src/untrusted/nacl/nacl_startup.h',
      'nacl_thread.h': 'src/untrusted/nacl/nacl_thread.h',
      'pnacl.h': 'src/untrusted/nacl/pnacl.h',
      'irt.h': 'src/untrusted/irt/irt.h',
      'irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
}


def InstallHeaders(tc_dst_inc, pepper_ver, tc_name):
  """Copies NaCl headers to expected locations in the toolchain."""
  tc_map = header_map[tc_name]
  for filename in tc_map:
    src = os.path.join(NACL_DIR, tc_map[filename])
    dst = os.path.join(tc_dst_inc, filename)
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


def main():
  platform = GetPlatform()
  arch = 'x86'
  skip_untar = 0
  skip_build = 0
  skip_tar = 0
  skip_examples = 0
  skip_headers = 0
  skip_make = 0
  force_archive = 0

  pepper_ver = build_utils.ChromeMajorVersion()
  clnumber = lastchange.FetchVersionInfo(None).revision
  print 'Building PEPPER %s at %s' % (pepper_ver, clnumber)

  BuildStep('Clean Pepper Dir')
  pepperdir = os.path.join(SRC_DIR, 'out', 'pepper_' + pepper_ver)
  if not skip_untar:
    RemoveDir(pepperdir)
    MakeDir(os.path.join(pepperdir, 'toolchain'))
    MakeDir(os.path.join(pepperdir, 'tools'))

  BuildStep('Untar Toolchains')
  tmpdir = os.path.join(SRC_DIR, 'out', 'tc_temp')
  tcname = platform + '_' + arch
  tmpdir = os.path.join(SRC_DIR, 'out', 'tc_temp')
  cygtar = os.path.join(NACL_DIR, 'build', 'cygtar.py')

    # Clean out the temporary toolchain untar directory
  if not skip_untar:
    RemoveDir(tmpdir)
    MakeDir(tmpdir)
    tcname = platform + '_' + arch

    # Untar the newlib toolchains
    tarfile = GetNewlibToolchain(platform, arch)
    Run([sys.executable, cygtar, '-C', tmpdir, '-xf', tarfile], cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'sdk', 'nacl-sdk')
    newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
    print "BUildbot mv %s to %s" % (srcdir, newlibdir)
    MoveDir(srcdir, newlibdir)
    print "Done with buildbot move"
    # Untar the glibc toolchains
    tarfile = GetGlibcToolchain(platform, arch)
    Run([sys.executable, cygtar, '-C', tmpdir, '-xf', tarfile], cwd=NACL_DIR)

    # Then rename/move it to the pepper toolchain directory
    srcdir = os.path.join(tmpdir, 'toolchain', tcname)
    glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')
    MoveDir(srcdir, glibcdir)
  else:
    newlibdir = os.path.join(pepperdir, 'toolchain', tcname + '_newlib')
    glibcdir = os.path.join(pepperdir, 'toolchain', tcname + '_glibc')

  BuildStep('SDK Items')
  if not skip_build:
    if arch == 'x86':
      Run(GetBuildArgs('newlib', newlibdir, 'x86', '32'), cwd=NACL_DIR)
      Run(GetBuildArgs('newlib', newlibdir, 'x86', '64'), cwd=NACL_DIR)

      Run(GetBuildArgs('glibc', glibcdir, 'x86', '32'), cwd=NACL_DIR)
      Run(GetBuildArgs('glibc', glibcdir, 'x86', '64'), cwd=NACL_DIR)
    else:
      ErrorExit('Missing arch %s' % arch)

  if not skip_headers:
    BuildStep('Copy Toolchain headers')
    if arch == 'x86':
      InstallHeaders(GetToolchainNaClInclude(newlibdir, 'x86'),
                     pepper_ver, 
                     'newlib')
      InstallHeaders(GetToolchainNaClInclude(newlibdir, 'x86'),
                     pepper_ver,
                     'glibc')
    else:
      ErrorExit('Missing arch %s' % arch)

  BuildStep('Copy make helpers')
  CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
        os.path.join(pepperdir, 'tools'))
  if platform == 'win':
    BuildStep('Add MAKE')
    http_download.HttpDownload(GSTORE + MAKE,
                               os.path.join(pepperdir, 'tools' ,'make.exe'))

  if not skip_examples:
    BuildStep('Copy examples')
    RemoveDir(os.path.join(pepperdir, 'examples'))
    CopyDir(os.path.join(SDK_SRC_DIR, 'examples'), pepperdir)


  tarname = 'naclsdk_' + platform + '.bz2'
  BuildStep('Tar Pepper Bundle')
  if not skip_tar:
    tarfile = os.path.join(OUT_DIR, 'naclsdk_' + platform + '.bz2')
    Run([sys.executable, cygtar, '-C', OUT_DIR, '-cjf', tarfile,
         'pepper_' + pepper_ver], cwd=NACL_DIR)

  # Archive on non-trybots.
  if force_archive or '-sdk' in os.environ.get('BUILDBOT_BUILDERNAME', ''):
    BuildStep('Archive build')
    Archive(tarname)

  if not skip_make:
    BuildStep('Test Build Examples')
    filelist = os.listdir(os.path.join(pepperdir, 'examples'))
    for filenode in filelist:
      dirnode = os.path.join(pepperdir, 'examples', filenode)
      makefile = os.path.join(dirnode, 'Makefile')
      if os.path.isfile(makefile):
        if platform == 'win':
          AddMakeBat(dirnode)
        print "\n\nMake: " + dirnode
        Run(['make', 'all', '-j8'], cwd=os.path.abspath(dirnode), shell=True)

  return 0


if __name__ == '__main__':
  sys.exit(main())

