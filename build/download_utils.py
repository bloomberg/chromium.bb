#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library to assist automatically downloading files.

This library is used by scripts that download tarballs, zipfiles, etc. as part
of the build process.
"""

import os.path
import shutil
import sys
import time


SOURCE_STAMP = 'SOURCE_URL'


# Designed to handle more general inputs than sys.platform because the platform
# name may come from the command line.
PLATFORM_COLLAPSE = {
    'windows': 'windows',
    'win32': 'windows',
    'cygwin': 'windows',
    'linux': 'linux',
    'linux2': 'linux',
    'linux3': 'linux',
    'darwin': 'mac',
    'mac': 'mac',
}

ARCH_COLLAPSE = {
    'i386'  : 'x86-32',
    'i686'  : 'x86-32',
    'x86_64': 'x86-64',
    'armv7l': 'arm',
}


def PlatformName(name=None):
  if name is None:
    name = sys.platform
  return PLATFORM_COLLAPSE[name]

def ArchName(name=None):
  if name is None:
    if PlatformName() == 'windows':
      # TODO(pdox): Figure out how to auto-detect 32-bit vs 64-bit Windows.
      name = 'i386'
    else:
      import platform
      name = platform.machine()
  return ARCH_COLLAPSE[name]

def EnsureFileCanBeWritten(filename):
  directory = os.path.dirname(filename)
  if not os.path.exists(directory):
    os.makedirs(directory)


def WriteData(filename, data):
  EnsureFileCanBeWritten(filename)
  f = open(filename, 'wb')
  f.write(data)
  f.close()


def WriteDataFromStream(filename, stream, chunk_size, verbose=True):
  EnsureFileCanBeWritten(filename)
  dst = open(filename, 'wb')
  try:
    while True:
      data = stream.read(chunk_size)
      if len(data) == 0:
        break
      dst.write(data)
      if verbose:
        # Indicate that we're still writing.
        sys.stdout.write('.')
        sys.stdout.flush()
  finally:
    if verbose:
      sys.stdout.write('\n')
    dst.close()


def StampMatches(stampfile, expected):
  try:
    f = open(stampfile, 'r')
    stamp = f.read()
    f.close()
    if stamp == expected:
      return "already up-to-date."
    elif stamp.startswith('manual'):
      return "manual override."
    return False
  except IOError:
    return False


def WriteStamp(stampfile, data):
  EnsureFileCanBeWritten(stampfile)
  f = open(stampfile, 'w')
  f.write(data)
  f.close()


def SourceIsCurrent(path, url):
  stampfile = os.path.join(path, SOURCE_STAMP)
  return StampMatches(stampfile, url)


def WriteSourceStamp(path, url):
  stampfile = os.path.join(path, SOURCE_STAMP)
  WriteStamp(stampfile, url)


def Retry(op, *args):
  # Windows seems to be prone to having commands that delete files or
  # directories fail.  We currently do not have a complete understanding why,
  # and as a workaround we simply retry the command a few times.
  # It appears that file locks are hanging around longer than they should.  This
  # may be a secondary effect of processes hanging around longer than they
  # should.  This may be because when we kill a browser sel_ldr does not exit
  # immediately, etc.
  # Virus checkers can also accidently prevent files from being deleted, but
  # that shouldn't be a problem on the bots.
  if sys.platform in ('win32', 'cygwin'):
    count = 0
    while True:
      try:
        op(*args)
        break
      except Exception:
        sys.stdout.write("FAILED: %s %s\n" % (op.__name__, repr(args)))
        count += 1
        if count < 5:
          sys.stdout.write("RETRY: %s %s\n" % (op.__name__, repr(args)))
          time.sleep(pow(2, count))
        else:
          # Don't mask the exception.
          raise
  else:
    op(*args)


def MoveDirCleanly(src, dst):
  RemoveDir(dst)
  MoveDir(src, dst)


def MoveDir(src, dst):
  Retry(shutil.move, src, dst)


def RemoveDir(path):
  if os.path.exists(path):
    Retry(shutil.rmtree, path)


def RemoveFile(path):
  if os.path.exists(path):
    Retry(os.unlink, path)
