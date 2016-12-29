# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
  """Warn when changing md_history without vulcanizing."""

  def _is_history_source_file(file):
    path = file.LocalPath()
    return (not path.endswith('externs.js') and
            not path.endswith('crisper.js') and
            not path.endswith('vulcanized.html') and
            (path.endswith('.js') or path.endswith('.html')))

  os_path = input_api.os_path
  earliest_vulcanize_change = min(os_path.getmtime(x) for x in
                                  ['app.vulcanized.html',
                                   'app.crisper.js',
                                   'lazy_load.vulcanized.html',
                                   'lazy_load.crisper.js'])

  source_files = input_api.AffectedFiles(file_filter=_is_history_source_file)
  latest_history_change = 0
  if source_files:
    latest_history_change = max(
        os_path.getmtime(os_path.basename(f.LocalPath())) for f in source_files)

  if latest_history_change > earliest_vulcanize_change:
    return [output_api.PresubmitPromptWarning(
        'Vulcanize must be run when changing files in md_history. See '
        'docs/vulcanize.md.')]
  return []
