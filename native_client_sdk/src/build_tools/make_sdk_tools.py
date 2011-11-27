#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create the base (auto)updater for the Native Client SDK"""

import py_compile
import optparse
import os
import shutil
import sys
import tarfile
import zipfile

from build_tools.sdk_tools import update_manifest
from build_tools.sdk_tools import sdk_update

NACL_SDK = 'nacl_sdk'


def ZipDirectory(dirpath, zippath):
  '''Create a zipfile from the contents of a given directory.

  The path of the resulting contents in the zipfile will match that of the
  last directory name in dirpath.

  Args:
    dirpath: Path to directory to add to zipfile
    zippath: filename of resulting zipfile
  '''
  zip = None
  try:
    zip = zipfile.ZipFile(zippath, 'w', zipfile.ZIP_DEFLATED)
    basedir = '%s%s' % (os.path.dirname(dirpath), os.sep)
    for root, dirs, files in os.walk(dirpath):
      if os.path.basename(root)[0] == '.':
        continue # skip hidden directories
      dirname = root.replace(basedir, '')
      for file in files:
        zip.write(os.path.join(root, file), os.path.join(dirname, file))
  finally:
    if zip:
      zip.close()


def WriteTarFile(outname, in_dir, tar_dir=''):
  '''Create a new compressed tarball from a given directory

  Args:
    outname: path and filename of the gzipped tar file
    in_dir: source directory that will be tar'd and gzipped
    tar_dir: root directory within the tarball'''
  tar_file = None
  try:
    tar_file = tarfile.open(outname, 'w:gz')
    tar_file.add(in_dir, tar_dir)
  finally:
    if tar_file:
      tar_file.close()


def MakeSdkTools(nacl_sdk_filename, sdk_tools_filename):
  '''Make the nacl_sdk and sdk_tools tarballs

  The nacl_sdk package contains these things:

  nacl_sdk/
    naclsdk(.bat)  - The main entry point for updating the SDK
    sdk_tools/
      sdk_update.py  - Performs the work in checking for updates
    python/
      python.exe  - (optional) python executable, shipped with Windows
      ... - other python files and directories
    sdk_cache/
      naclsdk_manifest.json  - manifest file with information about sdk_tools

  Args:
    nacl_sdk_filename: name of zipfile that the user directly downloads
    sdk_tools_filename: name of tarball that has the sdk_tools directory
  '''
  base_dir = os.path.abspath(os.path.dirname(__file__))
  base_dir_parent = os.path.dirname(base_dir)
  temp_dir = os.path.join(base_dir, NACL_SDK)
  if os.path.exists(temp_dir):
    shutil.rmtree(temp_dir)
  os.mkdir(temp_dir)
  for dir in ['sdk_tools', 'sdk_cache']:
    os.mkdir(os.path.join(temp_dir, dir))
  shutil.copy2(os.path.join(base_dir, 'naclsdk'), temp_dir)
  shutil.copy2(os.path.join(base_dir, 'naclsdk.bat'), temp_dir)
  with open(os.path.join(base_dir_parent, 'LICENSE'), "U") as source_file:
    text = source_file.read().replace("\n", "\r\n")
  with open(os.path.join(temp_dir, 'sdk_tools', 'LICENSE'), "wb") as dest_file:
    dest_file.write(text)

  tool_list = ['sdk_update.py', 'set_nacl_env.py']
  for tool in tool_list:
    shutil.copy2(os.path.join(base_dir, 'sdk_tools', tool),
                 os.path.join(temp_dir, 'sdk_tools'))
    py_compile.compile(os.path.join(temp_dir, 'sdk_tools', tool))

  update_manifest_options = [
       '--bundle-revision=%s' % sdk_update.MINOR_REV,
       '--bundle-version=%s' % sdk_update.MAJOR_REV,
       '--description=Native Client SDK Tools, revision %s.%s' % (
           sdk_update.MAJOR_REV, sdk_update.MINOR_REV),
       '--bundle-name=sdk_tools',
       '--recommended=yes',
       '--stability=stable',
       '--manifest-version=%s' % sdk_update.SDKManifest().MANIFEST_VERSION,
       '--manifest-file=%s' %
       os.path.join(temp_dir, 'sdk_cache', 'naclsdk_manifest.json')]
  if 0 != update_manifest.main(update_manifest_options):
    raise Exception('update_manifest terminated abnormally.')
  ZipDirectory(temp_dir, nacl_sdk_filename)
  WriteTarFile(sdk_tools_filename, os.path.join(temp_dir, 'sdk_tools'), '')
  shutil.rmtree(temp_dir, ignore_errors=True)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option(
      '-n', '--nacl-sdk', dest='nacl_sdk', default='nacl_sdk.zip',
      help='name of the resulting nacl_sdk zipfile')
  parser.add_option(
      '-s', '--sdk-tools', dest='sdk_tools', default='sdk_tools.tgz',
      help='name of the resulting sdk_tools tarball')
  (options, args) = parser.parse_args(argv)
  MakeSdkTools(options.nacl_sdk, options.sdk_tools)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
