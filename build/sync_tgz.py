#!/usr/bin/python2.4
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Keep a local directory in sync with a website tar file.

This module downloads a tgz, and expands it as needed.
It supports username and password with basic authentication.
"""

import os
import shutil
import subprocess
import sys
import tarfile

import http_download


def SyncTgz(url, target, maindir='sdk',
            username=None, password=None, verbose=True):
  """Download a file from a remote server.

  Args:
    url: A URL to download from.
    target: Directory to extract to and prefix to use for tgz file.
    username: Optional username for download.
    password: Optional password for download (ignored if no username).
    verbose: Flag indicating if status shut be printed.
  """
  shutil.rmtree(target, True)
  tgz_filename = target + '/.tgz'

  if verbose:
    print 'Downloading %s to %s...' % (url, tgz_filename)
  http_download.HttpDownload(url, tgz_filename,
    username=username, password=password, verbose=verbose)

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
    # Convert symlinks to hard links.
    subprocess.check_call(
        [os.path.join('tmptar', 'bash.exe'),
         '-c',
         '/tmptar/find -L ' + maindir + ' -type f -xtype l -print0 | ' +
         'while IFS="" read -r -d "" name; do ' +
         'if [[ -L "$name" ]];' +
         'then /tmptar/ln -Tf "$(/tmptar/readlink -f "$name")" ' +
         '"$name" ; fi ; done'], env=env)
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
  elif sys.platform == 'darwin':
    # TODO(khim): Replace with --warning=no-unknown-keyword when gnutar 1.23+
    # will be available.
    subprocess.check_call(
        ['bash', '-c',
         '/usr/bin/gnutar -xS' + verbosechar + 'pf ' + tgz_filename +
         ' -C ' + target + ' 2> /dev/null'])
  else:
    tgz = tarfile.open(tgz_filename, 'r')
    for m in tgz:
      if verbose:
        print m.name
      tgz.extract(m, target)
    tgz.close()
  os.remove(tgz_filename)

  if verbose:
    print 'Update complete.'
