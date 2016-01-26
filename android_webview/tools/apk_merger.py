#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Merges a 64-bit and a 32-bit APK into a single APK

"""

import os
import sys
import shutil
import zipfile
import filecmp
import tempfile
import argparse
import subprocess

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..')
SRC_DIR = os.path.abspath(SRC_DIR)
BUILD_ANDROID_GYP_DIR = os.path.join(SRC_DIR, 'build/android/gyp')
sys.path.append(BUILD_ANDROID_GYP_DIR)

import finalize_apk
from util import build_utils

class ApkMergeFailure(Exception):
  pass


def UnpackApk(file_name, dst):
  zippy = zipfile.ZipFile(file_name)
  zippy.extractall(dst)


def GetNonDirFiles(top, base_dir):
  """ Return a list containing all (non-directory) files in tree with top as
  root.

  Each file is represented by the relative path from base_dir to that file.
  If top is a file (not a directory) then a list containing only top is
  returned.
  """
  if os.path.isdir(top):
    ret = []
    for dirpath, _, filenames in os.walk(top):
      for filename in filenames:
        ret.append(
            os.path.relpath(os.path.join(dirpath, filename), base_dir))
    return ret
  else:
    return [os.path.relpath(top, base_dir)]


def GetDiffFiles(dcmp, base_dir):
  """ Return the list of files contained only in the right directory of dcmp.

  The files returned are represented by relative paths from base_dir.
  """
  copy_files = []
  for file_name in dcmp.right_only:
    copy_files.extend(
        GetNonDirFiles(os.path.join(dcmp.right, file_name), base_dir))

  # we cannot merge APKs with files with similar names but different contents
  if len(dcmp.diff_files) > 0:
    raise ApkMergeFailure('found differing files: %s in %s and %s' %
                          (dcmp.diff_files, dcmp.left, dcmp.right))

  if len(dcmp.funny_files) > 0:
    ApkMergeFailure('found uncomparable files: %s in %s and %s' %
                    (dcmp.funny_files, dcmp.left, dcmp.right))

  for sub_dcmp in dcmp.subdirs.itervalues():
    copy_files.extend(GetDiffFiles(sub_dcmp, base_dir))
  return copy_files


def CheckFilesExpected(actual_files, expected_files):
  """ Check that the lists of actual and expected files are the same. """
  file_set = set()
  for file_name in actual_files:
    base_name = os.path.basename(file_name)
    if base_name not in expected_files:
      raise ApkMergeFailure('Found unexpected file named %s.' %
                            file_name)
    if base_name in file_set:
      raise ApkMergeFailure('Duplicate file %s to add to APK!' %
                            file_name)
    file_set.add(base_name)

  if len(file_set) != len(expected_files):
    raise ApkMergeFailure('Missing expected files to add to APK!')


def AddDiffFiles(diff_files, tmp_dir_32, tmp_apk, expected_files):
  """ Insert files only present in 32-bit APK into 64-bit APK (tmp_apk). """
  old_dir = os.getcwd()
  # Move into 32-bit directory to make sure the files we insert have correct
  # relative paths.
  os.chdir(tmp_dir_32)
  try:
    for diff_file in diff_files:
      extra_flags = expected_files[os.path.basename(diff_file)]
      build_utils.CheckOutput(['zip', '-r', '-X', '--no-dir-entries',
                              tmp_apk, diff_file] + extra_flags)
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure(
        'Failed to add file %s to APK: %s' % (diff_file, e.output))
  finally:
    # Move out of 32-bit directory when done
    os.chdir(old_dir)


def RemoveMetafiles(tmp_apk):
  """ Remove all meta info to avoid certificate clashes """
  try:
    build_utils.CheckOutput(['zip', '-d', tmp_apk, 'META-INF/*'])
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure('Failed to delete Meta folder: ' + e.output)


def SignAndAlignApk(tmp_apk, signed_tmp_apk, new_apk, zipalign_path,
                    keystore_path, key_name, key_password,
                    page_align_shared_libraries):
  try:
    finalize_apk.JarSigner(
      keystore_path,
      key_name,
      key_password,
      tmp_apk,
      signed_tmp_apk)
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure('Failed to sign APK: ' + e.output)

  try:
    finalize_apk.AlignApk(zipalign_path,
                          page_align_shared_libraries,
                          signed_tmp_apk,
                          new_apk)
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure('Failed to align APK: ' + e.output)


def main():
  parser = argparse.ArgumentParser(
      description='Merge a 32-bit APK into a 64-bit APK')
  # Using type=os.path.abspath converts file paths to absolute paths so that
  # we can change working directory without affecting these paths
  parser.add_argument('--apk_32bit', required=True, type=os.path.abspath)
  parser.add_argument('--apk_64bit', required=True, type=os.path.abspath)
  parser.add_argument('--out_apk', required=True, type=os.path.abspath)
  parser.add_argument('--zipalign_path', required=True, type=os.path.abspath)
  parser.add_argument('--keystore_path', required=True, type=os.path.abspath)
  parser.add_argument('--key_name', required=True)
  parser.add_argument('--key_password', required=True)
  parser.add_argument('--shared_library', required=True)
  parser.add_argument('--page-align-shared-libraries', action='store_true')
  parser.add_argument('--uncompress-shared-libraries', action='store_true')
  args = parser.parse_args()

  tmp_dir = tempfile.mkdtemp()
  tmp_dir_64 = os.path.join(tmp_dir, '64_bit')
  tmp_dir_32 = os.path.join(tmp_dir, '32_bit')
  tmp_apk = os.path.join(tmp_dir, 'tmp.apk')
  signed_tmp_apk = os.path.join(tmp_dir, 'signed.apk')
  new_apk = args.out_apk

  # Expected files to copy from 32- to 64-bit APK together with an extra flag
  # setting the compression level of the file
  expected_files = {'snapshot_blob_32.bin': ['-0'],
                    'natives_blob_32.bin': ['-0'],
                    args.shared_library: []}

  if args.uncompress_shared_libraries:
    expected_files[args.shared_library] += ['-0']

  try:
    shutil.copyfile(args.apk_64bit, tmp_apk)

    # need to unpack APKs to compare their contents
    UnpackApk(args.apk_64bit, tmp_dir_64)
    UnpackApk(args.apk_32bit, tmp_dir_32)

    # TODO(sgurun) remove WebViewPlatformBridge.apk from this list crbug/580678
    dcmp = filecmp.dircmp(
        tmp_dir_64,
        tmp_dir_32,
        ignore=['META-INF', 'AndroidManifest.xml', 'WebViewPlatformBridge.apk'])

    diff_files = GetDiffFiles(dcmp, tmp_dir_32)

    # Check that diff_files match exactly those files we want to insert into
    # the 64-bit APK.
    CheckFilesExpected(diff_files, expected_files)

    RemoveMetafiles(tmp_apk)

    AddDiffFiles(diff_files, tmp_dir_32, tmp_apk, expected_files)

    SignAndAlignApk(tmp_apk, signed_tmp_apk, new_apk, args.zipalign_path,
                    args.keystore_path, args.key_name, args.key_password,
                    args.page_align_shared_libraries)

  except ApkMergeFailure as e:
    print e
    return 1
  finally:
    shutil.rmtree(tmp_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
