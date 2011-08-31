#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cook up tarballs/zipfile for distribution."""

import os
import re
import shutil
import stat
import subprocess
import sys


def RemoveIfExists(filename):
  try:
    os.remove(filename)
  except OSError:
    pass


def WindowsRemoveReadOnly(top):
  """Only on windows remove the read only attribute from a directory tree.

  Arguments:
    top: directory to change.
  """
  if not sys.platform == 'win32':
    return
  for root, dirs, files in os.walk(top, topdown=False):
    for name in files:
      os.chmod(os.path.join(root, name), stat.S_IWRITE)
    for name in dirs:
      os.chmod(os.path.join(root, name), stat.S_IWRITE)


def FindSVNRevision(path):
  try:
    info = subprocess.Popen(['svn', 'info'], shell=True,
                            stdout=subprocess.PIPE,
                            cwd=path).communicate()[0]
  except IOError:
    return 'UnknownRev'
  m = re.search('Revision: ([0-9]+)', info)
  if m:
    return m.group(1)
  else:
    return 'UnknownRev'


def DeleteAllMatching(in_dir, regex):
  """Delete all files matching a particular regex.

  Arguments:
    in_dir: directory to sweep.
    regex: file/directory expression to match.
  """
  for root, dirs, files in os.walk(in_dir, topdown=False):
    for name in files:
      if regex.match(name):
        os.remove(os.path.join(root, name))
    for name in dirs:
      if regex.match(name):
        shutil.rmtree(os.path.join(root, name))


def PickOuterName(build_dir):
  platform_name = {
      'win32': 'windows',
      'cygwin': 'windows',
      'darwin': 'mac',
      'linux': 'linux',
      'linux2': 'linux',
      'linux3': 'linux',
  }[sys.platform]
  return 'nacl_%s_%s' % (platform_name, FindSVNRevision(build_dir))


def CookTarball(tgz_name, build_mode):
  """Cook up a distribution tarball.

  Arguments:
    tgz_name: base name of the tar/zip to create.
    build_mode: --mode parameter for scons
  """
  # Pick where to construct the tarball.
  if sys.platform == 'win32':
    tmp_dir = 'c:/nacl_tarball'
  else:
    tmp_dir = '/tmp/nacl_tarball'

  # Find the root of this client.
  build_dir = os.path.abspath(os.path.dirname(__file__))
  src_dir = os.path.abspath(os.path.join(build_dir, '..', '..'))

  # Drop old working area.
  WindowsRemoveReadOnly(tmp_dir)
  shutil.rmtree(tmp_dir, ignore_errors=True)

  # Create working area directory.
  os.mkdir(tmp_dir)

  # Pick the root directory name in the destination.
  # TODO(bradnelson): consider switching this to use the following,
  # assuming this doesn't complicate the docs.
  # dst_dir = os.path.join(tmp_dir, PickOuterName(build_dir))
  dst_dir = os.path.join(tmp_dir, 'build')

  # Copy over everything.
  shutil.copytree(src_dir, dst_dir)
  WindowsRemoveReadOnly(dst_dir)

  # Drop old tarballs/zips.
  DeleteAllMatching(os.path.join(dst_dir, 'native_client', 'build'),
                    re.compile(r'.*\.tgz$'))
  DeleteAllMatching(os.path.join(dst_dir, 'native_client', 'build'),
                    re.compile(r'.*\.zip$'))

  # Drop .svn files.
  DeleteAllMatching(dst_dir, re.compile(r'^\.svn$'))

  # Drop gyp stuff.
  shutil.rmtree(os.path.join(dst_dir, 'sconsbuild'), ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'xcodebuild'), ignore_errors=True)
  for flavor in ['Debug', 'Release']:
    shutil.rmtree(os.path.join(dst_dir, 'native_client',
                               'build', flavor), ignore_errors=True)

  # Drop scons outputs.
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'scons-out'),
                ignore_errors=True)

  # Drop tools BUILD intermediate output.
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'tools', 'BUILD'),
                ignore_errors=True)

  # Drop any toolchain present.
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'src',
                             'third_party', 'nacl_sdk'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'compiler'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'toolchain'),
                ignore_errors=True)

  # Pick scons version.
  if sys.platform == 'win32':
    scons = os.path.join(dst_dir, 'native_client', 'scons.bat')
  else:
    scons = os.path.join(dst_dir, 'native_client', 'scons')

  # Pick doxygen version.
  doxy_version = {
      'win32': 'win',
      'cygwin': 'win',
      'darwin': 'osx',
      'linux': 'linux',
      'linux2': 'linux',
  }[sys.platform]
  doxy_path = os.path.normpath('../third_party/doxygen/%s/doxygen' %
                               doxy_version)

  # Build the tools.
  tool_platform = {
      'win32': 'win_x86',
      'cygwin': 'win_x86',
      'darwin': 'mac_x86',
      'linux': 'linux_x86',
      'linux2': 'linux_x86',
  }[sys.platform]
  cmd = ('PATH=$PATH:/cygdrive/c/cygwin/bin '
         'MAKEINFO=`pwd`/makeinfo_dummy '
         'make '
         'SDKLOC=`pwd`/../toolchain/%(tool_platform)s/sdk '
         'HAMMER=scons') % {'tool_platform': tool_platform}
  if sys.platform == 'win32':
    cmd = "c:\\cygwin\\bin\\bash -c '%s'" % cmd
  ret = subprocess.call(cmd, shell=True,
                        cwd=os.path.join(dst_dir, 'native_client', 'tools'))
  if ret:
    return ret

  # Drop tools BUILD intermediate output.
  shutil.rmtree(os.path.join(dst_dir, 'native_client', 'tools', 'BUILD'),
                ignore_errors=True)

  # Build the desired version.
  ret = subprocess.call([scons,
                        '--mode='+build_mode,
                        '--verbose',
                        'DOXYGEN=%s' % doxy_path],
                        cwd=os.path.join(dst_dir, 'native_client'))
  if ret:
    return ret

  # Drop items only needed for toolchain build.
  shutil.rmtree(os.path.join(dst_dir, 'third_party', 'binutils'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'third_party', 'newlib'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'third_party', 'gcc'), ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'third_party', 'gdb'), ignore_errors=True)

  # Drop doxygen if present.
  shutil.rmtree(os.path.join(dst_dir, 'third_party', 'doxygen'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(dst_dir, 'doxygen.DEPS'), ignore_errors=True)

  # Pare back scons-out.
  for ext in [re.compile(r'.*\.o$'),
              re.compile(r'.*\.obj$', re.I),
              re.compile(r'.*\.a$'),
              re.compile(r'.*\.lib$', re.I),
              re.compile(r'.*\.sconsign.*')]:
    DeleteAllMatching(os.path.join(dst_dir, 'native_client', 'scons-out'), ext)

  # Zip/tar it up.
  if sys.platform in ['win32', 'cygwin']:
    out_file = os.path.abspath(tgz_name + '.zip')
    RemoveIfExists(out_file)
    ret = subprocess.call(['zip', '-vr', out_file, '.'],
                          cwd=os.path.join(tmp_dir))
    if ret:
      return ret
  else:
    out_file = os.path.abspath(tgz_name + '.tgz')
    ret = subprocess.call(['tar', 'cvfz', out_file, './'],
                          cwd=os.path.join(tmp_dir))
    if ret:
      return ret
  # Success.
  return 0


def main(argv):
  if len(argv) != 3:
    print 'USAGE: ./cook_tarball.sh <tgz_name> <build_mode>'
    return 1

  return CookTarball(tgz_name=argv[1], build_mode=argv[2])


if __name__ == '__main__':
  sys.exit(main(sys.argv))
