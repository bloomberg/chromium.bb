# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(joaodasilva): remove this file. http://crbug.com/327345

import itertools

def _CheckPolicyProtobufs(input_api, output_api):
  # List of pairs (A, B) where A should equal B.
  file_pairs = [
    ( 'chrome/browser/policy/proto/chromeos/chrome_device_policy.proto',
      'chrome/browser/chromeos/policy/proto/chrome_device_policy.proto' ),
    ( 'chrome/browser/policy/proto/chromeos/install_attributes.proto',
      'chrome/browser/chromeos/policy/proto/install_attributes.proto' ),
    ( 'chrome/browser/policy/proto/cloud/chrome_extension_policy.proto',
      'components/policy/proto/chrome_extension_policy.proto' ),
    ( 'chrome/browser/policy/proto/cloud/device_management_backend.proto',
      'components/policy/proto/device_management_backend.proto' ),
    ( 'chrome/browser/policy/proto/cloud/device_management_local.proto',
      'components/policy/proto/device_management_local.proto' ),
    ( 'chrome/browser/policy/proto/PRESUBMIT.py',
      'components/policy/proto/PRESUBMIT.py' ),
    ( 'chrome/browser/chromeos/policy/proto/PRESUBMIT.py',
      'components/policy/proto/PRESUBMIT.py' ),
  ]

  root = input_api.change.RepositoryRoot()
  results = []

  for file_a, file_b in file_pairs:
    path_a = input_api.os_path.join(root, *file_a.split('/'))
    path_b = input_api.os_path.join(root, *file_b.split('/'))
    with open(path_a, 'r') as f_a:
      content_a = f_a.readlines()[3:]
    with open(path_b, 'r') as f_b:
      content_b = f_b.readlines()[3:]
    if content_a != content_b:
      # If you get this error then check |file_pairs| and make sure that the
      # contents of the files in each pair match.
      results.append(output_api.PresubmitError(
          '%s must equal %s. This is temporary until http://crbug.com/327345 '
          'is fixed.' % (file_a, file_b)))

  # If new files are added then |file_pairs| must be updated.
  existing = frozenset(itertools.chain(*file_pairs))
  for f in input_api.AffectedFiles():
    if f.LocalPath() not in existing:
      # If you get this error then add an entry for the new files to
      # |file_pairs|.
      results.append(output_api.PresubmitError(
          'Please add an entry for %s to %s/PRESUBMIT.py' %
          (f.LocalPath(), input_api.PresubmitLocalPath())))

  return results


def _CommonChecks(input_api, output_api):
  return _CheckPolicyProtobufs(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
