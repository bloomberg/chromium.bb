# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
  """Warn when changing md_history without vulcanizing."""

  def _is_md_history_file(path):
    return (path.startswith('chrome/browser/resources/md_history') and
            (not path.endswith('externs.js')) and
            (path.endswith('js') or path.endswith('html')))

  def _affects_file(filename, paths):
    return any([filename in path for path in paths])

  paths = [x.LocalPath() for x in input_api.change.AffectedFiles()]
  vulcanize_changes = (_affects_file('md_history/app.vulcanized.html', paths) or
                       _affects_file('md_history/app.crisper.js', paths))
  history_changes = filter(_is_md_history_file, paths)

  if history_changes and not vulcanize_changes:
    return [output_api.PresubmitPromptWarning(
        'Vulcanize must be run when changing files in md_history. See '
        'docs/vulcanize.md.')]
  return []
