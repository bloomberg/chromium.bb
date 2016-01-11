# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def _GetChromiumSrcDir(input_api):
  return input_api.os_path.abspath(input_api.os_path.join(
      input_api.PresubmitLocalPath(), '..', '..', '..', '..'))


def LoadSupport(input_api):
  if 'cloud_storage' not in globals():
    # Avoid leaking changes to global sys.path.
    _old_sys_path = sys.path
    try:
      catapult_base_path = input_api.os_path.join(
          _GetChromiumSrcDir(input_api), 'third_party', 'catapult',
          'catapult_base')
      sys.path = [catapult_base_path] + sys.path
      from catapult_base import cloud_storage
      globals()['cloud_storage'] = cloud_storage
    finally:
      sys.path = _old_sys_path

  return globals()['cloud_storage']


def _GetFilesNotInCloud(input_api):
  """Searches for .sha1 files and checks to see if they have already
  been uploaded Cloud Storage. Returns a list of those that have not.
  """
  hash_paths = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    hash_path = affected_file.AbsoluteLocalPath()
    _, extension = input_api.os_path.splitext(hash_path)
    if extension == '.sha1':
      hash_paths.append(hash_path)
  if not hash_paths:
    return []

  cloud_storage = LoadSupport(input_api)

  # Look in both buckets, in case the user uploaded the file manually.
  hashes_in_cloud_storage = cloud_storage.List(cloud_storage.PUBLIC_BUCKET)
  try:
    hashes_in_cloud_storage += cloud_storage.List(cloud_storage.INTERNAL_BUCKET)
  except (cloud_storage.PermissionError, cloud_storage.CredentialsError):
    pass

  files = []
  for hash_path in hash_paths:
    file_hash = cloud_storage.ReadHash(hash_path)
    if file_hash not in hashes_in_cloud_storage:
      files.append((hash_path, file_hash))

  return files


def _VerifyFilesInCloud(input_api, output_api):
  """Fails presubmit if any .sha1 files have not been previously uploaded to
  Cloud storage.
  """
  results = []
  hash_paths = _GetFilesNotInCloud(input_api)
  file_paths = []
  for hash_path, _ in hash_paths:
    results.append(output_api.PresubmitError(
        'Attemping to commit hash file, but corresponding '
        'data file is not in Cloud Storage: %s' % hash_path))
    file_paths.append(input_api.os_path.splitext(hash_path)[0])

  if len(file_paths) > 0:
    upload_script_path = input_api.os_path.join(
        _GetChromiumSrcDir(input_api), 'tools', 'telemetry', 'cloud_storage')
    results.append(output_api.PresubmitError(
          'To upload missing files, Run: \n'
          '%s upload %s google-only' %
          (upload_script_path, ' '.join(file_paths))))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = _VerifyFilesInCloud(input_api, output_api)
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = _VerifyFilesInCloud(input_api, output_api)
  return results
