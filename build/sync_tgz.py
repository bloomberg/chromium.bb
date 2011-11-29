#!/usr/bin/python2.4
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Keep a local directory in sync with a website tar file.

This module downloads a tgz, and expands it as needed.
It supports username and password with basic authentication.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import urllib2

import download_utils
import http_download

def CreateCygwinSymlink(filepath, target):
  """create a cygwin 1.7 style link.

  Arguments:
    filepath: The filepath of the symlink file.
    target: The path to the target of the symlink.

  Generates a Cygwin style symlink by creating a SYSTEM tagged
  file with the !<link> marker an unicode path.
  """
  lnk = open(filepath, 'w')
  uni = '!<symlink>\xff\xfe'
  uni += ''.join([chr + '\x00' for chr in m.linkname])
  uni += '\x00\x00'
  lnk.write(uni)
  lnk.close()
  subprocess.call(['cmd', '/C', 'attrib.exe', '+S', filepath])


def _HashFileHandle(fh):
  """sha1 of a file like object.

  Arguments:
    fh: file handle like object to hash.
  Returns:
    sha1 as a string.
  """
  hasher = hashlib.sha1()
  try:
    while True:
      data = fh.read(4096)
      if not data:
        break
      hasher.update(data)
  finally:
    fh.close()
  return hasher.hexdigest()


def HashFile(filename):
  """sha1 a file on disk.

  Arguments:
    filename: filename to hash.
  Returns:
    sha1 as a string.
  """
  fh = open(filename, 'rb')
  return _HashFileHandle(fh)


def HashUrl(url):
  """sha1 the data at an url.

  Arguments:
    url: url to download from.
  Returns:
    sha1 of the data at the url.
  """
  fh = urllib2.urlopen(url)
  return _HashFileHandle(fh)


class HashError(Exception):
  def __init__(self, download_url, expected_hash, actual_hash):
    self.download_url = download_url
    self.expected_hash = expected_hash
    self.actual_hash = actual_hash

  def __str__(self):
    return 'Got hash "%s" but expected hash "%s" for "%s"' % (
        self.actual_hash, self.expected_hash, self.download_url)


def SyncTgz(url, tar_dir, dst_dir=None, username=None, password=None,
            verbose=True, hash=None, keep=False):
  """Download a file from a remote server.
  Args:
    url: A URL to download from.
    tar_dir: Directory for tar download.
    dst_dir: Directory to extract to.
    username: Optional username for download.
    password: Optional password for download (ignored if no username).
    verbose: Flag indicating if status shut be printed.
    hash: sha1 of expected download (don't check if hash is None).
    keep: Keep the archive, do not delete when done.
  """

  verbose=True
  filename = url.split('/')[-1]
  tar_filepath = os.path.join(tar_dir, filename)
  if not dst_dir:
    dst_dir = tar_dir

  # Crate the directory if it doesn't exist
  if not os.path.isdir(tar_dir): os.makedirs(tar_dir)
  if not os.path.isdir(dst_dir): os.makedirs(dst_dir)

  if verbose:
    print "Archive URL: %s" % url
    print "Archive file: %s" % tar_filepath
    print "Extract path: %s" % dst_dir
  # Clean out old directory
  if verbose:
    print 'Cleaning out %s' % dst_dir
  shutil.rmtree(dst_dir, True)
  if verbose:
    print 'Downloading %s to %s...' % (url, tar_filepath)
  http_download.HttpDownload(url, tar_filepath,
    username=username, password=password, verbose=verbose)

  tar_hash = HashFile(tar_filepath)
  if hash and hash != tar_hash:
    raise HashError(actual_hash=tar_hash, expected_hash=hash, download_url=url)

  if verbose:
    print 'Extracting from %s...' % tar_filepath

  tar = tarfile.open(tar_filepath, 'r')
  links = []
  for m in tar:
    if verbose:
      typeinfo = '?'
      lnk = ''
      if m.issym():
        typeinfo = 'S'
        lnk = '-> ' + m.linkname
      if m.islnk():
        typeinfo = 'H'
        lnk = '-> ' + m.linkname
      if m.isdir():
        typeinfo = 'D'
      if m.isfile():
        typeinfo = 'F'
      print '%s : %s %s' % (typeinfo, m.name, lnk)

    # For symlinks in Windows we create Cygwin 1.7 style symlinks since the
    # toolchain is Cygwin based.  For all other tar items, or platforms we
    # go ahead and extract it normally.
    if m.issym() and sys.platform == 'win32':
      CreateCygwinSymlink(os.path.join(dst_dir, m.name))
    else:
      tar.extract(m, dst_dir)

  tar.close()
  if not keep:
    os.remove(tar_filepath)

  if verbose:
    print 'Update complete.'
