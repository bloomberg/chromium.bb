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
  parser.add_option('--arch',
                    help='Target architecture',
                    dest='arch',
                    default='x86-32')
  parser.add_option('--sdk_version',
                    help='SDK Version (pepper_[X], r[X])'
                         ' (default=buildbot revision)',
                    dest='sdk_version',
                    default='')
  (options, args) = parser.parse_args(args[1:])

  assert sys.platform.find('linux') != -1

  buildbot_revision = os.environ.get('BUILDBOT_REVISION', '')

  buildbot_common.BuildStep('Clean Old SDK')
  buildbot_common.MakeDir('mono_build')
  os.chdir('mono_build')
  buildbot_common.RemoveDir('pepper_*')

  buildbot_common.BuildStep('Setup New SDK')
  sdk_dir = None
  sdk_revision = None
  if options.sdk_version == '':
    assert buildbot_revision
    sdk_revision = buildbot_revision.split(':')[0]
    url = 'gs://nativeclient-mirror/nacl/nacl_sdk/'\
          'trunk.%s/naclsdk_linux.bz2' % sdk_revision
    buildbot_common.Run([buildbot_common.GetGsutil(), 'cp', url, '.'])
    tar_file = None
    try:
      tar_file = tarfile.open('naclsdk_linux.bz2')
      pepper_dir = os.path.commonprefix(tar_file.getnames())
      tar_file.extractall()
      sdk_dir = os.path.join(os.getcwd(), pepper_dir)
    finally:
      if tar_file:
        tar_file.close()
  else:
    buildbot_common.ErrorExit('sdk_version not yet supported')

  assert sdk_dir
  assert sdk_revision

  buildbot_common.BuildStep('Checkout Mono')
  # TODO(elijahtaylor): Get git URL from master/trigger to make this
  # more flexible for building from upstream and release branches.
  git_url = 'git://github.com/elijahtaylor/mono.git'
  git_dir = 'nacl-mono'
  git_rev = None
  if buildbot_revision:
    git_rev = buildbot_revision.split(':')[1]
  if not os.path.exists(git_dir):
    buildbot_common.Run(['git', 'clone', git_url, git_dir])
    os.chdir(git_dir) 
  else:
    os.chdir(git_dir) 
    buildbot_common.Run(['git', 'fetch'])
  if git_rev:
    buildbot_common.Run(['git', 'checkout', git_rev])

  arch_to_bitsize = {'x86-32': '32',
                     'x86-64': '64'}
  arch_to_output_folder = {'x86-32': 'runtime-build',
                           'x86-64': 'runtime64-build'}
  arch_to_install_folder = {'x86-32': 'naclmono-i686',
                            'x86-64': 'naclmono-x86_64'}

  buildbot_common.BuildStep('Configure Mono')
  os.environ['NACL_SDK_ROOT'] = sdk_dir
  os.environ['TARGET_BITSIZE'] = arch_to_bitsize[options.arch]
  buildbot_common.Run(['./autogen.sh']) 
  buildbot_common.Run(['make', 'distclean'])

  buildbot_common.BuildStep('Build/Install Mono')
  # TODO(elijahtaylor): This script only exists in my fork.
  # This script should live in the SDK's build_tools/
  os.chdir('nacl')
  buildbot_common.RemoveDir(arch_to_install_folder[options.arch])
  nacl_interp_script = os.path.join(os.getcwd(), 'nacl_interp_loader_sdk.sh')
  os.environ['NACL_INTERP_LOADER'] = nacl_interp_script
  buildbot_common.Run(['./nacl-runtime-mono.sh'])

  buildbot_common.BuildStep('Test Mono')
  os.chdir(arch_to_output_folder[options.arch])
  buildbot_common.Run(['make', 'check', '-j8'])
  os.chdir('..')

  buildbot_common.BuildStep('Archive Build')
  tar_file = None
  tar_path = arch_to_install_folder[options.arch] + '.bz2'
  buildbot_common.RemoveFile(tar_path)
  try:
    tar_file = tarfile.open(tar_path, mode='w:bz2')
    tar_file.add(arch_to_install_folder[options.arch])
  finally:
    if tar_file:
      tar_file.close()

  buildbot_common.Archive(tar_path,
      'nativeclient-mirror/nacl/nacl_sdk/%s' % sdk_revision)

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
