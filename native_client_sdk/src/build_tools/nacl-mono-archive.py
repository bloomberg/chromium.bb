#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys
import tarfile

import buildbot_common


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--install-dir',
                    help='Install Directory',
                    dest='install_dir',
                    default='naclmono')
  parser.add_option('--tar-path',
                    help='Tarfile path',
                    dest='tar_path',
                    default='naclmono_%pepperrev%.bz2')
  parser.add_option('--sdk-revision',
                    help='SDK Revision',
                    dest='sdk_revision',
                    default=None)
  parser.add_option('--pepper-revision',
                    help='Pepper revision',
                    dest='pepper_revision',
                    default=None)
  parser.add_option('--skip-upload',
                    help='Skips upload step',
                    action="store_true",
                    dest='skip_upload')
  (options, args) = parser.parse_args(args[1:])

  if not options.sdk_revision:
    buildbot_common.ErrorExit('--sdk-revision is required')
  if not options.pepper_revision:
    buildbot_common.ErrorExit('--pepper-revision is required')

  options.tar_path = options.tar_path.replace('%pepperrev%',
                                              options.pepper_revision)

  install_folders = ['bin', 'etc', 'include', 'lib', 'lib32', 'share']

  root_dir = 'mono_pepper_' + options.pepper_revision

  buildbot_common.BuildStep('Archive Build')
  tar_file = None
  buildbot_common.RemoveFile(options.tar_path)
  try:
    tar_file = tarfile.open(options.tar_path, mode='w:bz2')
    for subfolder in install_folders:
      tar_file.add(os.path.join(options.install_dir, subfolder),
                   arcname=os.path.join(root_dir, subfolder))
  finally:
    if tar_file:
      tar_file.close()

  if not options.skip_upload:
    buildbot_common.Archive(os.path.basename(options.tar_path),
        'nativeclient-mirror/nacl/nacl_sdk/%s' % options.sdk_revision,
        cwd=os.path.dirname(os.path.abspath(options.tar_path)))

if __name__ == '__main__':
  sys.exit(main(sys.argv))
