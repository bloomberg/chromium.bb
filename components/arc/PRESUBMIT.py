# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_CPP_WHITE_LIST = (r'.+\.cc$', r'.+\.h$')


def CheckCallback(input_api, output_api):
  """Discourages the use of deprecated base::{Bind,Callback,Closure}."""
  # This contains a list of regex and a human-readable error message.
  patterns = [
      (r'\bbase::Bind\(',
       'uses base::Bind. Please consider using base::Bind{Once,Repeating} '
       'instead.'),
      (r'\bbase::Callback<',
       'uses base::Callback. Please consider using '
       'base::{Once,Repeating}Callback instead'),
      (r'\bbase::Closure\b',
       'uses base::Closure. Please consider using '
       'base::{Once,Repeating}Closure instead'),
  ]
  problems = []
  def SourceFilter(affected_file):
    return input_api.FilterSourceFile(affected_file, _CPP_WHITE_LIST,
                                      input_api.DEFAULT_BLACK_LIST)
  for f in input_api.AffectedSourceFiles(SourceFilter):
    for line_number, line in f.ChangedContents():
      for pattern in patterns:
        if input_api.re.search(pattern[0], line):
          problems.append(
              '%s:%d %s' % (f.LocalPath(), line_number, pattern[1]))
  if problems:
    return [output_api.PresubmitPromptWarning(
        'base::Bind, base::Closure, and base::Callback are in the process ' +
        'of being deprecated. See crbug.com/714018 for more information ' +
        'about the migration\n',
        items=problems)]
  return []


def CheckMakeUnique(input_api, output_api):
  """Encourage to use std::make_unique, instead of base::MakeUnique."""
  errors = []
  def SourceFilter(affected_file):
    return input_api.FilterSourceFile(affected_file, _CPP_WHITE_LIST,
                                      input_api.DEFAULT_BLACK_LIST)
  for f in input_api.AffectedSourceFiles(SourceFilter):
    for line_number, line in f.ChangedContents():
      if input_api.re.search(r'\bbase::MakeUnique<', line):
        errors.append(output_api.PresubmitError(
            '%s:%d uses base::MakeUnique. Use std::make_unique instead.' %
            (f.LocalPath(), line_number)))
  return errors


def CheckUniquePtr(input_api, output_api):
  # Runs CheckUniquePtr in cc/PRESUBMIT.py.
  # Probably, we'd like to promote the check to somewhere to be shared.
  presubmit_path = (
      input_api.change.RepositoryRoot() + '/cc/PRESUBMIT.py')
  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec(presubmit_content, global_vars)
  return global_vars['CheckUniquePtr'](input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api)
  results += CheckCallback(input_api, output_api)
  results += CheckUniquePtr(input_api, output_api)
  results += CheckMakeUnique(input_api, output_api)
  return results
