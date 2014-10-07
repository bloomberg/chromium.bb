# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/base.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import re

BASE_SOURCE_FILES=(r'^base/.*\.(cc|h|mm)$',)

def _CheckNoInterfacesInBase(input_api, output_api):
  """Checks to make sure no files in libbase.a have |@interface|."""
  pattern = input_api.re.compile(r'^\s*@interface', input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('base/') and
        not "/test/" in f.LocalPath() and
        not f.LocalPath().endswith('_unittest.mm') and
        not f.LocalPath().endswith('mac/sdk_forward_declarations.h')):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitError(
        'Objective-C interfaces or categories are forbidden in libbase. ' +
        'See http://groups.google.com/a/chromium.org/group/chromium-dev/' +
        'browse_thread/thread/efb28c10435987fd',
        files) ]
  return []


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoInterfacesInBase(input_api, output_api))
  return results

def _CheckOverrideFinal(input_api, output_api,
                       whitelist=BASE_SOURCE_FILES, blacklist=None):
  """Make sure new lines of code don't use the OVERRIDE or FINAL macros."""

  # TODO(mostynb): remove this check once the macros are removed
  # from base/compiler_specific.h.

  errors = []

  source_file_filter = lambda x: input_api.FilterSourceFile(
    x, white_list=BASE_SOURCE_FILES, black_list=None)

  override_files = []
  final_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')

    # "override" and "final" should be used instead of OVERRIDE/FINAL now.
    if re.search(r"\bOVERRIDE\b", contents):
      override_files.append(f.LocalPath())

    if re.search(r"\bFINAL\b", contents):
      final_files.append(f.LocalPath())

  if override_files:
    return [output_api.PresubmitError(
      'These files use OVERRIDE instead of using override:',
      items=override_files)]
  if final_files:
    return [output_api.PresubmitError(
      'These files use FINAL instead of using final:',
      items=final_files)]

  return []

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckOverrideFinal(input_api, output_api))
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def GetPreferredTryMasters(project, change):
  return {
    'tryserver.chromium.linux': {
      'linux_chromium_rel_swarming': set(['defaulttests']),
    },
    'tryserver.chromium.mac': {
      'mac_chromium_rel_swarming': set(['defaulttests']),
    },
    'tryserver.chromium.win': {
      'win_chromium_rel_swarming': set(['defaulttests']),
    }
  }
