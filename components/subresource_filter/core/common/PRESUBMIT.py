# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for subresource_filter component's core/common directory.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CheckIndexedRulesetVersion(input_api, output_api):
  """ Checks that IndexedRuleset format version is modified when necessary.

  Whenever any of the following files is changed:
   - components/subresource_filter/core/common/indexed_ruleset.cc
   - components/url_pattern_index/flat/*.fbs
   - components/url_pattern_index/url_pattern_index.cc
  and kIndexedFormatVersion constant stays intact, this check returns a
  presubmit warning to make sure the value should not be updated.
  """

  indexed_ruleset_changed = False
  indexed_ruleset_version_changed = False

  for affected_file in input_api.AffectedFiles():
    path = affected_file.LocalPath()
    if (not 'components/subresource_filter/core/common' in path and
        not 'components/url_pattern_index/flat' in path):
      continue
    basename = input_api.basename(path)

    if (basename == 'indexed_ruleset.cc' or basename == 'url_pattern_index.cc'
        or basename.endswith('.fbs')):
      indexed_ruleset_changed = True
    if basename == 'indexed_ruleset.cc':
      for (_, line) in affected_file.ChangedContents():
        if 'kIndexedFormatVersion =' in line:
          indexed_ruleset_version_changed = True
          break

  if indexed_ruleset_changed and not indexed_ruleset_version_changed:
    return [output_api.PresubmitPromptWarning(
        'Please make sure that UrlPatternIndex/IndexedRuleset modifications in '
        '*.fbs and url_pattern_index.cc/indexed_ruleset.cc do not require '
        'updating RulesetIndexer::kIndexedFormatVersion.')]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckIndexedRulesetVersion(input_api, output_api)
