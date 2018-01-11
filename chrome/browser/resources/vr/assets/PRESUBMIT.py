# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def IsNewer(old_version, new_version):
  return (old_version and new_version and
          (old_version.major < new_version.major or
           (old_version.major == new_version.major and
            old_version.minor < new_version.minor)))


def CheckVersion(input_api, output_api):
  """Checks that
  - the version was upraded if assets files were changed,
  - the version was not downgraded.
  """
  sys.path.append(input_api.PresubmitLocalPath())
  import parse_version

  old_version = None
  new_version = None
  changed_assets = False
  changed_version = False
  for file in input_api.AffectedFiles():
    basename = input_api.os_path.basename(file.LocalPath())
    extension = input_api.os_path.splitext(basename)[1][1:].strip().lower()
    if (extension == 'sha1' or basename == 'vr_assets_component_files.json'):
      changed_assets = True
    if (basename == 'VERSION'):
      changed_version = True
      old_version = parse_version.ParseVersion(file.OldContents())
      new_version = parse_version.ParseVersion(file.NewContents())

  local_version_filename = input_api.os_path.join(
      input_api.os_path.dirname(input_api.AffectedFiles()[0].LocalPath()),
      'VERSION')

  if changed_version and (not old_version or not new_version):
    return [
        output_api.PresubmitError(
            'Cannot parse version in \'%s\'.' % local_version_filename)
    ]

  version_upgraded = IsNewer(old_version, new_version)
  if changed_assets and not version_upgraded:
    return [
        output_api.PresubmitError(
            'Must increment version in \'%s\' when '
            'updating VR assets.' % local_version_filename)
    ]
  if changed_version and not version_upgraded:
    return [
        output_api.PresubmitError(
            'Must not downgrade version in \'%s\'.' % local_version_filename)
    ]

  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckVersion(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckVersion(input_api, output_api)
