#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Cook up tarballs/zipfile for distribution."""


import os
import re
import shutil
import stat
import subprocess
import sys


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


def CookTarball(tgz_name, build_mode):
  """Cook up a distribution tarball.

  Arguments:
    tgz_name: base name of the tar/zip to create.
    build_mode: --mode parameter for scons
  """
  # Pick where to construct the tarball.
  if sys.platform in ['win32']:
    tmp_dir = 'c:\\nacl_tarball'
  else:
    tmp_dir = '/tmp/nacl_tarball'

  # Find the root of this client.
  build_dir = os.path.abspath(os.path.dirname(__file__))
  src_dir = os.path.abspath(os.path.join(build_dir, '..', '..'))

  # Drop old working area.
  WindowsRemoveReadOnly(tmp_dir)
  shutil.rmtree(tmp_dir)

  # Copy over everything.
  shutil.copytree(src_dir, tmp_dir)
  WindowsRemoveReadOnly(tmp_dir)

  # Drop .svn files.
  DeleteAllMatching(tmp_dir, re.compile(r'^\.svn$'))

  # Drop gyp stuff.
  shutil.rmtree(os.path.join(tmp_dir, 'sconsbuild'), ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'xcodebuild'), ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'build', 'Debug'), ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'build', 'Release'), ignore_errors=True)

  # Drop scons outputs.
  shutil.rmtree(os.path.join(tmp_dir, 'native_client', 'scons-out'),
                ignore_errors=True)

  # Pick scons version.
  if sys.platform in ['win32']:
    scons = os.path.join(tmp_dir, 'native_client', 'scons.bat')
  else:
    scons = os.path.join(tmp_dir, 'native_client', 'scons')

  # Pick doxygen version.
  doxy_version = {
      'win32': 'win',
      'cygwin': 'win',
      'darwin': 'osx',
      'linux': 'linux',
      'linux2': 'linux',
  }[sys.platform]

  # Build the desired version.
  subprocess.call([scons, '--mode='+build_mode,
                   '--verbose', '--download',
                   'DOXYGEN=../third_party/doxygen/%s/doxygen' % doxy_version],
                  cwd=os.path.join(tmp_dir, 'native_client'))

  # Drop items only needed for sdk.
  shutil.rmtree(os.path.join(tmp_dir, 'third_party', 'binutils'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'third_party', 'newlib'),
                ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'third_party', 'gcc'), ignore_errors=True)
  shutil.rmtree(os.path.join(tmp_dir, 'third_party', 'gdb'), ignore_errors=True)

  # Drop nacl_sdk.
  shutil.rmtree(os.path.join(tmp_dir, 'native_client', 'src', 'third_party',
                             'nacl_sdk'), ignore_errors=True)

  # Pare back scons-out.
  for ext in [re.compile(r'.*\.o$'),
              re.compile(r'.*\.obj$', re.I),
              re.compile(r'.*\.a$'),
              re.compile(r'.*\.lib$', re.I),
              re.compile(r'.*\.sconsign.*')]:
    DeleteAllMatching(os.path.join(tmp_dir, 'native_client', 'scons-out'), ext)

  # Tar it up.
  if sys.platform in ['win32']:
    subprocess.call(['zip', '-vr',
                     os.path.abspath(tgz_name + '.zip'), '.'],
                    cwd=os.path.join(tmp_dir))
  else:
    subprocess.call(['tar', 'cvfz',
                     os.path.abspath(tgz_name + '.tgz'), './'],
                    cwd=os.path.join(tmp_dir))


def main(argv):
  if len(argv) != 3:
    print 'USAGE: ./cook_tarball.sh <tgz_name> <build_mode>'
    return 1

  return CookTarball(tgz_name=argv[1], build_mode=argv[2])


if __name__ == '__main__':
  sys.exit(main(sys.argv))
