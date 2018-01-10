#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates dependencies managed by CIPD."""

import argparse
import os
import subprocess
import sys
import tempfile

_SRC_ROOT = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))


def cipd_ensure(root, service_url, ensure_file):

  def is_windows():
    return sys.platform in ('cygwin', 'win32')

  cipd_binary = 'cipd'
  if is_windows():
    cipd_binary = 'cipd.bat'

  with tempfile.NamedTemporaryFile() as tmp_stdouterr:
    retcode = subprocess.call(
        [cipd_binary, 'ensure',
         '-ensure-file', ensure_file,
         '-root', root,
         '-service-url', service_url],
        shell=is_windows(),
        stderr=subprocess.STDOUT,
        stdout=tmp_stdouterr)
    if retcode:
      tmp_stdouterr.seek(0)
      for line in tmp_stdouterr:
        print line,
    return retcode


def build_ensure_file(sub_files):
  pieces = []
  for sub in sub_files:
    pieces.append('### Sourced from //%s' % os.path.relpath(sub, _SRC_ROOT))
    with open(sub) as f:
      pieces.append(f.read())
  return '\n'.join(pieces)


def main():
  parser = argparse.ArgumentParser(
      description='Updates CIPD-managed dependencies based on the given OS.')

  parser.add_argument(
      '--keep-generated-file',
      action='store_true',
      help='Don\'t delete the generated ensure file, if any, on success.')
  parser.add_argument(
      '--chromium-root',
      required=True,
      help='Root directory for dependency.')
  parser.add_argument(
      '--service-url',
      help='The url of the CIPD service.',
      default='https://chrome-infra-packages.appspot.com')
  parser.add_argument(
      '--ensure-file',
      action='append',
      type=os.path.realpath,
      required=True,
      help='The path to the ensure file. If given multiple times, this will '
           'concatenate the given ensure files before passing them to cipd.')
  args = parser.parse_args()

  build_own_ensure_file = len(args.ensure_file) > 1
  if build_own_ensure_file:
    ensure_file_contents = build_ensure_file(args.ensure_file)
    with tempfile.NamedTemporaryFile(suffix='.ensure') as ensure_file:
      ensure_file.write(ensure_file_contents)
      ensure_file_name = ensure_file.name
      ensure_file.delete = False
  else:
    ensure_file_name = args.ensure_file[0]

  retcode = cipd_ensure(args.chromium_root, args.service_url, ensure_file_name)
  if retcode:
    if build_own_ensure_file:
      print 'Ensure failed; generated input file is at', ensure_file_name
    return retcode

  if build_own_ensure_file:
    if args.keep_generated_file:
      print 'Leaving the generated .ensure file at', ensure_file_name
    else:
      os.unlink(ensure_file_name)
  return 0

if __name__ == '__main__':
  sys.exit(main())
