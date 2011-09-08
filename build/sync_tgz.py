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


def SyncTgz(url, target, username=None, password=None, verbose=True, hash=None,
            save_path=None):
  """Download a file from a remote server.

  Args:
    url: A URL to download from.
    target: Directory to extract to and prefix to use for tgz file.
    username: Optional username for download.
    password: Optional password for download (ignored if no username).
    verbose: Flag indicating if status shut be printed.
    hash: sha1 of expected download (don't check if hash is None).
    save_path: Optional place to save the downloaded tarball.  If this is None,
        then the tarball is downloaded to a temporary file which is deleted.
  """
  shutil.rmtree(target, True)
  os.makedirs(target)
  tgz_filename = target + '/.tgz'

  if verbose:
    print 'Downloading %s to %s...' % (url, tgz_filename)
  http_download.HttpDownload(url, tgz_filename,
    username=username, password=password, verbose=verbose)

  tgz_hash = HashFile(tgz_filename)
  if hash and hash != tgz_hash:
    raise HashError(actual_hash=tgz_hash, expected_hash=hash, download_url=url)

  # If saving the tarball, also produce a manifest file that can be parsed by
  # the SDK installer generator script.  Save this manifest file as
  # |save_path|.manifest.  Do this because the 'xz' compression format is not
  # supported by the scripts used in the SDK.
  save_manifest = None
  if save_path:
    if not os.path.exists(os.path.dirname(save_path)):
      os.makedirs(os.path.dirname(save_path))
    save_manifest = os.path.abspath(os.path.normpath(save_path + '.manifest'))

  if verbose:
    print 'Extracting from %s...' % tgz_filename
  if verbose:
    verbosechar = 'v'
  else:
    verbosechar = ''
  if sys.platform == 'win32':
    os.makedirs(os.path.join(target, 'tmptar'))
    tarfiles = ['cyggcc_s-1.dll', 'cygiconv-2.dll', 'cygintl-8.dll',
                'cyglzma-1.dll', 'cygncursesw-10.dll', 'cygreadline7.dll',
                'cygwin1.dll', 'bash.exe', 'bzip2.exe', 'find.exe',
                'gzip.exe', 'ln.exe', 'readlink.exe', 'tar.exe', 'xz.exe']
    for filename in tarfiles:
      http_download.HttpDownload(
        'http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/'
        'cygwin_mirror/cygwin/' + filename,
        os.path.join(target, 'tmptar', filename), verbose=verbose)
    saveddir = os.getcwd()
    os.chdir(target)
    env = os.environ.copy()
    env['LC_ALL'] = 'C'
    compress = 'gzip'
    if url.endswith('.xz'):
      compress = 'xz'
    subprocess.check_call(
        [os.path.join('tmptar', 'tar.exe'),
         '--use-compress-program', '/tmptar/' + compress,
         '-xS' + verbosechar + 'opf', '../.tgz'], env=env)
    if save_manifest:
      # Save the manifest file.  Use 'wb' as the open mode so that python on
      # Windows doesn't add spurious CRLF line endings.
      manifest = open(save_manifest, 'wb')
      subprocess.check_call(
        [os.path.join('tmptar', 'tar.exe'),
         '--use-compress-program', '/tmptar/' + compress,
         '-tSvopf', '../.tgz'], env=env, stdout=manifest)
      manifest.close()
    os.chdir(saveddir)
    # Some antivirus software can prevent the removal - print message, but
    # don't stop.
    for filename in tarfiles:
      count = 0
      while True:
        try:
          os.remove(os.path.join(target, 'tmptar', filename))
          break
        except EnvironmentError, e:
          if count > 10:
            if verbose:
              print "Can not remove %s: %s" % (filename, e.strerror)
            break
    try:
      os.rmdir(os.path.join(target, 'tmptar'))
    except EnvironmentError, e:
      if verbose:
        print "Can not rmdir %s: %s" % (os.path.join(target, 'tmptar'),
                                          e.strerror)
  elif sys.platform == 'linux2':
    compression_char = 'z'
    if url.endswith('.xz'):
      compression_char = 'J'
    subprocess.check_call(['tar', '-xS' + verbosechar + compression_char + 'pf',
                           tgz_filename, '-C', target])
    if save_manifest:
      # Save the manifest file.
      manifest = open(save_manifest, 'w')
      subprocess.check_call(['tar', '-tSv' + compression_char + 'pf',
                             tgz_filename], stdout=manifest)
      manifest.close()
  elif sys.platform == 'darwin':
    # TODO(khim): Replace with --warning=no-unknown-keyword when gnutar 1.23+
    # will be available.
    subprocess.check_call(
        ['bash', '-c',
         '/usr/bin/gnutar -xS' + verbosechar + 'pf ' + tgz_filename +
         ' -C ' + target + ' 2> /dev/null'])
    if save_manifest:
      # Save the manifest file.
      manifest = open(save_manifest, 'w')
      subprocess.check_call(
          ['bash', '-c',
           '/usr/bin/gnutar -tSvpf ' + tgz_filename + ' 2> /dev/null'],
          stdout=manifest)
      manifest.close()
  else:
    tgz = tarfile.open(tgz_filename, 'r')
    for m in tgz:
      if verbose:
        print m.name
      tgz.extract(m, target)
    # Note: tarballs that can be processed with the tarfile module do not
    # require a special manifest file.  No need to create one in this case.
    tgz.close()
  # If the tarball is supposed to be saved, move into place.  Otherwise, delete
  # it.  Note that the file is moved like this because the CygWin-based
  # Windows code above expects the tarball to be in a specific hard-coded
  # location and have a specific name.
  if save_path:
    download_utils.RemoveFile(save_path)
    shutil.move(tgz_filename, save_path)
  else:
    os.remove(tgz_filename)

  if verbose:
    print 'Update complete.'
