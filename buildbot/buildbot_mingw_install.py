#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install a snapshoted copy of MinGW in c:\MinGW."""

import hashlib
import os
import shutil
import subprocess
import sys
import zipfile


MINGW_VERSION = '2013_10_09'
MINGW_HASH = '6731a7f08c17dbf2e38a6f3b00fed7852102bfd8'
MINGW_GS_PATH = 'gs://nativeclient-private/mingw_%s.zip' % MINGW_VERSION
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
MINGW_DIR = os.path.join(NACL_DIR, 'MinGW')
MINGW_VERSION_STAMP = os.path.join(MINGW_DIR, 'VERSION_STAMP')
MINGW_VERSION_STAMP_PATTERN = '%s with hash %s'
MINGW_DST_ZIP = os.path.join(NACL_DIR, 'MinGW.zip')
MINGW_EXTRACT_DIR = MINGW_DIR + '_extract'


def Main():
  if sys.platform != 'win32':
    print 'Non-windows systems not supported!'
    sys.exit(1)

  # Skip install if version stamp of the right version exists.
  if os.path.exists(MINGW_VERSION_STAMP):
    with open(MINGW_VERSION_STAMP) as fh:
      expected = MINGW_VERSION_STAMP_PATTERN % (MINGW_GS_PATH, MINGW_HASH)
      content = fh.read()
      if content ==  expected:
        print 'MinGW already up to date.'
        return

  # Remove old zip if any.
  if os.path.exists(MINGW_DST_ZIP):
    print 'Removing %s' % MINGW_DST_ZIP
    os.remove(MINGW_DST_ZIP)

  # Remove old version if any.
  if os.path.exists(MINGW_DIR):
    print 'Removing %s' % MINGW_DIR
    shutil.rmtree(MINGW_DIR)

  # Remove old extract directory if any.
  if os.path.exists(MINGW_EXTRACT_DIR):
    print 'Removing %s' % MINGW_EXTRACT_DIR
    shutil.rmtree(MINGW_EXTRACT_DIR)

  # Download current version.
  print 'Downloading from %s to %s' % (MINGW_GS_PATH, MINGW_DST_ZIP)
  subprocess.check_call([
      os.environ.get('GSUTIL', 'gsutil'), 'cp', MINGW_GS_PATH, MINGW_DST_ZIP])

  # Check the sha1sum.
  print 'Checking that sha1 of %s is %s' % (MINGW_DST_ZIP, MINGW_HASH)
  m = hashlib.sha1()
  with open(MINGW_DST_ZIP, 'rb') as fh:
    m.update(fh.read())
  if m.hexdigest() != MINGW_HASH:
    raise Exception('Invalid zip sha1 %s does not match expected %s' % (
        m.hexdigest(), MINGW_HASH))

  # Unzip it.
  print 'Unziping %s to %s' % (MINGW_DST_ZIP, MINGW_EXTRACT_DIR)
  zp = zipfile.ZipFile(MINGW_DST_ZIP, 'r')
  zp.extractall(MINGW_EXTRACT_DIR)
  zp.close()

  # Remove zip file.
  print 'Removing %s' % MINGW_DST_ZIP
  os.remove(MINGW_DST_ZIP)

  # Setup fstab for hermetic use.
  # Msys expects mingw to be located at /mingw. To provide this in a file
  # system location independent way, /etc/fstab (inside Msys) can be setup to
  # mount MinGW at /mingw.
  fstab = os.path.join(
      MINGW_EXTRACT_DIR, 'MinGW', 'msys', '1.0', 'etc', 'fstab')
  print 'Writing %s' % fstab
  with open(fstab, 'w') as fh:
    fh.write('%s /mingw' % MINGW_DIR)

  # Move into place.
  src = os.path.join(MINGW_EXTRACT_DIR, 'MinGW')
  print 'Moving %s to %s' % (src, MINGW_DIR)
  shutil.move(src, MINGW_DIR)
  os.rmdir(MINGW_EXTRACT_DIR)

  # Writing version stamp.
  print 'Writing version stamp to %s' % MINGW_VERSION_STAMP
  with open(MINGW_VERSION_STAMP, 'w') as fh:
    fh.write(MINGW_VERSION_STAMP_PATTERN % (MINGW_GS_PATH, MINGW_HASH))

  print 'Done.'


if __name__ == '__main__':
  Main()
