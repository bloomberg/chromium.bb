#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

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
    'darwin': 'mac',
    'mac': 'mac',
}

ARCH_COLLAPSE = {
    'i386'  : 'x86-32',
    'i686'  : 'x86-32',
    'x86_64': 'x86-64',
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
    return stamp == expected
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
  if sys.platform in ('win32', 'cygwin'):
    for i in range(5):
      try:
        op(*args)
        break
      except Exception:
        sys.stdout.write("RETRY: %s %s\n" % (op.__name__, repr(args)))
        time.sleep(pow(2, i))
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
