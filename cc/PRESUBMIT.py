# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for cc.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import re
import string

CC_SOURCE_FILES=(r'^cc/.*\.(cc|h)$',)
CC_PERF_TEST =(r'^.*_perftest.*\.(cc|h)$',)

def CheckChangeLintsClean(input_api, output_api):
  input_api.cpplint._cpplint_state.ResetErrorCounts()  # reset global state
  source_filter = lambda x: input_api.FilterSourceFile(
    x, white_list=CC_SOURCE_FILES, black_list=None)
  files = [f.AbsoluteLocalPath() for f in
           input_api.AffectedSourceFiles(source_filter)]
  level = 1  # strict, but just warn

  for file_name in files:
    input_api.cpplint.ProcessFile(file_name, level)

  if not input_api.cpplint._cpplint_state.error_count:
    return []

  return [output_api.PresubmitPromptWarning(
    'Changelist failed cpplint.py check.')]

def CheckAsserts(input_api, output_api, white_list=CC_SOURCE_FILES, black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x, white_list, black_list)

  assert_files = []
  notreached_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # WebKit ASSERT() is not allowed.
    if re.search(r"\bASSERT\(", contents):
      assert_files.append(f.LocalPath())
    # WebKit ASSERT_NOT_REACHED() is not allowed.
    if re.search(r"ASSERT_NOT_REACHED\(", contents):
      notreached_files.append(f.LocalPath())

  if assert_files:
    return [output_api.PresubmitError(
      'These files use ASSERT instead of using DCHECK:',
      items=assert_files)]
  if notreached_files:
    return [output_api.PresubmitError(
      'These files use ASSERT_NOT_REACHED instead of using NOTREACHED:',
      items=notreached_files)]
  return []

def CheckStdAbs(input_api, output_api,
                white_list=CC_SOURCE_FILES, black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            white_list,
                                                            black_list)

  using_std_abs_files = []
  found_fabs_files = []
  missing_std_prefix_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if re.search(r"using std::f?abs;", contents):
      using_std_abs_files.append(f.LocalPath())
    if re.search(r"\bfabsf?\(", contents):
      found_fabs_files.append(f.LocalPath());
    # The following regular expression in words says:
    # "if there is no 'std::' behind an 'abs(' or 'absf(',
    # or if there is no 'std::' behind a 'fabs(' or 'fabsf(',
    # then it's a match."
    if re.search(r"((?<!std::)(\babsf?\()|(?<!std::)(\bfabsf?\())", contents):
      missing_std_prefix_files.append(f.LocalPath())

  result = []
  if using_std_abs_files:
    result.append(output_api.PresubmitError(
        'These files have "using std::abs" which is not permitted.',
        items=using_std_abs_files))
  if found_fabs_files:
    result.append(output_api.PresubmitError(
        'std::abs() should be used instead of std::fabs() for consistency.',
        items=found_fabs_files))
  if missing_std_prefix_files:
    result.append(output_api.PresubmitError(
        'These files use abs(), absf(), fabs(), or fabsf() without qualifying '
        'the std namespace. Please use std::abs() in all places.',
        items=missing_std_prefix_files))
  return result

def CheckSpamLogging(input_api,
                     output_api,
                     white_list=CC_SOURCE_FILES,
                     black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            white_list,
                                                            black_list)

  log_info = []
  printf = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if re.search(r"\bD?LOG\s*\(\s*INFO\s*\)", contents):
      log_info.append(f.LocalPath())
    if re.search(r"\bf?printf\(", contents):
      printf.append(f.LocalPath())

  if log_info:
    return [output_api.PresubmitError(
      'These files spam the console log with LOG(INFO):',
      items=log_info)]
  if printf:
    return [output_api.PresubmitError(
      'These files spam the console log with printf/fprintf:',
      items=printf)]
  return []

def CheckPassByValue(input_api,
                     output_api,
                     white_list=CC_SOURCE_FILES,
                     black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            white_list,
                                                            black_list)

  local_errors = []

  # Well-defined simple classes containing only <= 4 ints, or <= 2 floats.
  pass_by_value_types = ['base::Time',
                         'base::TimeTicks',
                         'gfx::Point',
                         'gfx::PointF',
                         'gfx::Rect',
                         'gfx::Size',
                         'gfx::SizeF',
                         'gfx::Vector2d',
                         'gfx::Vector2dF',
                         ]

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    match = re.search(
      r'\bconst +' + '(?P<type>(%s))&' %
        string.join(pass_by_value_types, '|'),
      contents)
    if match:
      local_errors.append(output_api.PresubmitError(
        '%s passes %s by const ref instead of by value.' %
        (f.LocalPath(), match.group('type'))))
  return local_errors

def CheckTodos(input_api, output_api):
  errors = []

  source_file_filter = lambda x: x
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if ('FIX'+'ME') in contents:
      errors.append(f.LocalPath())

  if errors:
    return [output_api.PresubmitError(
      'All TODO comments should be of the form TODO(name).',
      items=errors)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckAsserts(input_api, output_api)
  results += CheckStdAbs(input_api, output_api)
  results += CheckSpamLogging(input_api, output_api, black_list=CC_PERF_TEST)
  results += CheckPassByValue(input_api, output_api)
  results += CheckChangeLintsClean(input_api, output_api)
  results += CheckTodos(input_api, output_api)
  return results

def GetPreferredTrySlaves(project, change):
  return [
    'linux_layout_rel',
    ]
