#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import optparse
import os
import urllib2
import sys
import time


# Print a dot every time this number of bytes is read.
PROGRESS_SPACING = 128 * 1024


def ReadFile(filename):
  fh = open(filename, 'r')
  try:
    return fh.read()
  finally:
    fh.close()


def WriteFile(filename, data):
  fh = open(filename, 'w')
  try:
    fh.write(data)
  finally:
    fh.close()


def HashFile(filename):
  hasher = hashlib.sha1()
  fh = open(filename, 'rb')
  try:
    while True:
      data = fh.read(4096)
      if len(data) == 0:
        break
      hasher.update(data)
  finally:
    fh.close()
  return hasher.hexdigest()


def CopyStream(input_stream, output_stream):
  """Copies the contents of input_stream to output_stream.  Prints
  dots to indicate progress.
  """
  bytes_read = 0
  dots_printed = 0
  while True:
    data = input_stream.read(4096)
    if len(data) == 0:
      break
    output_stream.write(data)
    bytes_read += len(data)
    if bytes_read / PROGRESS_SPACING > dots_printed:
      sys.stdout.write('.')
      sys.stdout.flush()
      dots_printed += 1


def RenameWithRetry(old_path, new_path):
  # Renames of files that have recently been closed are known to be
  # unreliable on Windows, because virus checkers like to keep the
  # file open for a little while longer.  This tends to happen more
  # for files that look like Windows executables, which does not apply
  # to our files, but we retry the rename here just in case.
  if sys.platform in ('win32', 'cygwin'):
    for i in range(5):
      try:
        if os.path.exists(new_path):
          os.remove(new_path)
        os.rename(old_path, new_path)
        return
      except Exception, exn:
        sys.stdout.write('Rename failed with %r.  Retrying...\n' % str(exn))
        sys.stdout.flush()
        time.sleep(1)
    raise Exception('Unabled to rename irt file')
  else:
    os.rename(old_path, new_path)


def DownloadFile(dest_path, url):
  url_path = '%s.url' % dest_path
  temp_path = '%s.temp' % dest_path
  if os.path.exists(url_path) and ReadFile(url_path).strip() == url:
    # The URL matches that of the file we previously downloaded, so
    # there should be nothing to do.
    return
  sys.stdout.write('Downloading %r to %r\n' % (url, dest_path))
  output_fh = open(temp_path, 'wb')
  stream = urllib2.urlopen(url)
  CopyStream(stream, output_fh)
  output_fh.close()
  sys.stdout.write(' done\n')
  if os.path.exists(url_path):
    os.unlink(url_path)
  RenameWithRetry(temp_path, dest_path)
  WriteFile(url_path, url + '\n')
  stream.close()


def DownloadFileWithRetry(dest_path, url):
  for i in range(5):
    try:
      DownloadFile(dest_path, url)
      break
    except urllib2.HTTPError, exn:
      if exn.getcode() == 404:
        raise
      sys.stdout.write('Download failed with error %r.  Retrying...\n'
                       % str(exn))
      sys.stdout.flush()
      time.sleep(1)


def EvalDepsFile(path):
  scope = {'Var': lambda name: scope['vars'][name]}
  execfile(path, {}, scope)
  return scope


def Main():
  parser = optparse.OptionParser()
  parser.add_option(
      '--base_url', dest='base_url',
      # For a view of this site that includes directory listings, see:
      # http://gsdview.appspot.com/nativeclient-archive2/
      # (The trailing slash is required.)
      default=('http://commondatastorage.googleapis.com/'
               'nativeclient-archive2/irt'),
      help='Base URL from which to download.')
  parser.add_option(
      '--nacl_revision', dest='nacl_revision',
      help='Download an IRT binary that was built from this '
        'SVN revision of Native Client.')
  parser.add_option(
      '--file_hash', dest='file_hashes', action='append', nargs=2, default=[],
      metavar='ARCH HASH',
      help='ARCH gives the name of the architecture (e.g. "x86_32") for '
        'which to download an IRT binary.  '
        'HASH gives the expected SHA1 hash of the file.')
  options, args = parser.parse_args()
  if len(args) != 0:
    parser.error('Unexpected arguments: %r' % args)

  if options.nacl_revision is None and len(options.file_hashes) == 0:
    # The script must have been invoked directly with no arguments,
    # rather than being invoked by gclient.  In this case, read the
    # DEPS file ourselves rather than having gclient pass us values
    # from DEPS.
    deps_data = EvalDepsFile(os.path.join('src', 'DEPS'))
    options.nacl_revision = deps_data['vars']['nacl_revision']
    options.file_hashes = [
        ('x86_32', deps_data['vars']['nacl_irt_hash_x86_32']),
        ('x86_64', deps_data['vars']['nacl_irt_hash_x86_64']),
        ]

  nacl_dir = os.path.join('src', 'native_client')
  if not os.path.exists(nacl_dir):
    # If "native_client" is not present, this might be because the
    # developer has put '"src/native_client": None' in their
    # '.gclient' file, because they don't want to build Chromium with
    # Native Client support.  So don't create 'src/native_client',
    # because that would interfere with checking it out from SVN
    # later.
    sys.stdout.write(
        'The directory %r does not exist: skipping downloading binaries '
        'for Native Client\'s IRT library\n' % nacl_dir)
    return
  if len(options.file_hashes) == 0:
    sys.stdout.write('No --file_hash arguments given: nothing to update\n')

  new_deps = []
  for arch, expected_hash in options.file_hashes:
    url = '%s/r%s/irt_%s.nexe' % (options.base_url,
                                  options.nacl_revision,
                                  arch)
    dest_dir = os.path.join(nacl_dir, 'irt_binaries')
    if not os.path.exists(dest_dir):
      os.makedirs(dest_dir)
    dest_path = os.path.join(dest_dir, 'nacl_irt_%s.nexe' % arch)
    DownloadFileWithRetry(dest_path, url)
    downloaded_hash = HashFile(dest_path)
    if downloaded_hash != expected_hash:
      sys.stdout.write(
          'Hash mismatch: the file downloaded from URL %r had hash %r, '
          'but we expected %r\n' % (url, downloaded_hash, expected_hash))
      new_deps.append('  "nacl_irt_hash_%s": "%s",\n'
                      % (arch, downloaded_hash))

  if len(new_deps) > 0:
    sys.stdout.write('\nIf you have changed nacl_revision, the DEPS file '
                     'probably needs to be updated with the following:\n%s\n'
                     % ''.join(new_deps))
    sys.exit(1)


if __name__ == '__main__':
  Main()
