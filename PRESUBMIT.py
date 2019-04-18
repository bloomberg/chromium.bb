# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CommonChecks(input_api, output_api):
  results = []
  # TODO(issues/43): Probably convert this to python so we can give more
  # detailed errors.
  presubmit_sh_result = input_api.subprocess.call(
      input_api.PresubmitLocalPath() + '/PRESUBMIT.sh')
  if presubmit_sh_result != 0:
    results.append(output_api.PresubmitError('PRESUBMIT.sh failed'))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
