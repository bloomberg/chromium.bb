# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for scanning source files to determine code authorship.
"""

import itertools


def FindFiles(input_api, root_dir, start_paths_list, excluded_dirs_list):
  """Similar to UNIX utility find(1), searches for files in the directories.
  Automatically leaves out only source code files.
  Args:
    input_api: InputAPI, as in presubmit scripts.
    root_dir: The root directory, to which all other paths are relative.
    start_paths_list: The list of paths to start search from. Each path can
      be a file or a directory.
    excluded_dirs_list: The list of directories to skip.
  Returns:
    The list of source code files found, relative to |root_dir|.
  """
  dirs_blacklist = ['/' + d + '/' for d in excluded_dirs_list]
  def IsBlacklistedDir(d):
    for item in dirs_blacklist:
      if item in d:
        return True
    return False

  files_whitelist_re = input_api.re.compile(
    r'\.(asm|c(c|pp|xx)?|h(h|pp|xx)?|p(l|m)|xs|sh|php|py(|x)'
    '|rb|idl|java|el|sc(i|e)|cs|pas|inc|js|pac|html|dtd|xsl|mod|mm?'
    '|tex|mli?)$')
  files = []

  base_path_len = len(root_dir)
  for path in start_paths_list:
    full_path = input_api.os_path.join(root_dir, path)
    if input_api.os_path.isfile(full_path):
      if files_whitelist_re.search(path):
        files.append(path)
    else:
      for dirpath, dirnames, filenames in input_api.os_walk(full_path):
        # Remove excluded subdirs for faster scanning.
        for item in dirnames[:]:
          if IsBlacklistedDir(
              input_api.os_path.join(dirpath, item)[base_path_len + 1:]):
            dirnames.remove(item)
        for filename in filenames:
          filepath = \
              input_api.os_path.join(dirpath, filename)[base_path_len + 1:]
          if files_whitelist_re.search(filepath) and \
              not IsBlacklistedDir(filepath):
            files.append(filepath)
  return files


class _GeneratedFilesDetector(object):
  GENERATED_FILE = 'GENERATED FILE'
  NO_COPYRIGHT = '*No copyright*'

  def __init__(self, input_api):
    self.python_multiline_string_double_re = \
      input_api.re.compile(r'"""[^"]*(?:"""|$)', flags=input_api.re.MULTILINE)
    self.python_multiline_string_single_re = \
      input_api.re.compile(r"'''[^']*(?:'''|$)", flags=input_api.re.MULTILINE)
    self.automatically_generated_re = input_api.re.compile(
      r'(All changes made in this file will be lost'
      '|DO NOT (EDIT|delete this file)'
      '|Generated (at|automatically|data)'
      '|Automatically generated'
      '|\Wgenerated\s+(?:\w+\s+)*file\W)', flags=input_api.re.IGNORECASE)

  def IsGeneratedFile(self, header):
    header = header.upper()
    if '"""' in header:
      header = self.python_multiline_string_double_re.sub('', header)
    if "'''" in header:
      header = self.python_multiline_string_single_re.sub('', header)
    # First do simple strings lookup to save time.
    if 'ALL CHANGES MADE IN THIS FILE WILL BE LOST' in header:
      return True
    if 'DO NOT EDIT' in header or 'DO NOT DELETE' in header or \
        'GENERATED' in header:
      return self.automatically_generated_re.search(header)
    return False


class _CopyrightsScanner(object):
  @staticmethod
  def StaticInit(input_api):
    _CopyrightsScanner._c_comment_re = \
      input_api.re.compile(r'''"[^"\\]*(?:\\.[^"\\]*)*"''')
    _CopyrightsScanner._copyright_indicator = \
      r'(?:copyright|copr\.|\xc2\xa9|\(c\))'
    _CopyrightsScanner._full_copyright_indicator_re = input_api.re.compile(
      r'(?:\W|^)' + _CopyrightsScanner._copyright_indicator + \
      r'(?::\s*|\s+)(\w.*)$', input_api.re.IGNORECASE)
    _CopyrightsScanner._copyright_disindicator_re = input_api.re.compile(
      r'\s*\b(?:info(?:rmation)?|notice|and|or)\b', input_api.re.IGNORECASE)

  def __init__(self, input_api):
    self.max_line_numbers_proximity = 3
    self.last_a_item_line_number = -200
    self.last_b_item_line_number = -100
    self.re = input_api.re

  def _CloseLineNumbers(self, a, b):
    return 0 <= a - b <= self.max_line_numbers_proximity

  def MatchLine(self, line_number, line):
    if '"' in line:
      line = _CopyrightsScanner._c_comment_re.sub('', line)
    upcase_line = line.upper()
    # Record '(a)' and '(b)' last occurences in C++ comments.
    # This is to filter out '(c)' used as a list item inside C++ comments.
    # E.g. "// blah-blah (a) blah\n// blah-blah (b) and (c) blah"
    cpp_comment_idx = upcase_line.find('//')
    if cpp_comment_idx != -1:
      if upcase_line.find('(A)') > cpp_comment_idx:
        self.last_a_item_line_number = line_number
      if upcase_line.find('(B)') > cpp_comment_idx:
        self.last_b_item_line_number = line_number
    # Fast bailout, uses the same patterns as _copyright_indicator regexp.
    if not 'COPYRIGHT' in upcase_line and not 'COPR.' in upcase_line \
        and not '\xc2\xa9' in upcase_line:
      c_item_index = upcase_line.find('(C)')
      if c_item_index == -1:
        return None
      if c_item_index > cpp_comment_idx and \
          self._CloseLineNumbers(line_number,
                                 self.last_b_item_line_number) and \
          self._CloseLineNumbers(self.last_b_item_line_number,
                                 self.last_a_item_line_number):
        return None
    copyr = None
    m = _CopyrightsScanner._full_copyright_indicator_re.search(line)
    if m and \
        not _CopyrightsScanner._copyright_disindicator_re.match(m.group(1)):
      copyr = m.group(0)
      # Prettify the authorship string.
      copyr = self.re.sub(r'([,.])?\s*$/', '', copyr)
      copyr = self.re.sub(
        _CopyrightsScanner._copyright_indicator, '', copyr, \
        flags=self.re.IGNORECASE)
      copyr = self.re.sub(r'^\s+', '', copyr)
      copyr = self.re.sub(r'\s{2,}', ' ', copyr)
      copyr = self.re.sub(r'\\@', '@', copyr)
    return copyr


def FindCopyrights(input_api, root_dir, files_to_scan):
  """Determines code autorship, and finds generated files.
  Args:
    input_api: InputAPI, as in presubmit scripts.
    root_dir: The root directory, to which all other paths are relative.
    files_to_scan: The list of file names to scan.
  Returns:
    The list of copyrights associated with each of the files given.
    If the certain file is generated, the corresponding list consists a single
    entry -- 'GENERATED_FILE' string. If the file has no copyright info,
    the corresponding list contains 'NO_COPYRIGHT' string.
  """
  generated_files_detector = _GeneratedFilesDetector(input_api)
  _CopyrightsScanner.StaticInit(input_api)
  copyrights = []
  for file_name in files_to_scan:
    linenum = 0
    header = []
    file_copyrights = []
    scanner = _CopyrightsScanner(input_api)
    contents = input_api.ReadFile(
      input_api.os_path.join(root_dir, file_name), 'r')
    for l in contents.split('\n'):
      linenum += 1
      if linenum <= 25:
        header.append(l)
      c = scanner.MatchLine(linenum, l)
      if c:
        file_copyrights.append(c)
    if generated_files_detector.IsGeneratedFile('\n'.join(header)):
      copyrights.append([_GeneratedFilesDetector.GENERATED_FILE])
    elif file_copyrights:
      copyrights.append(file_copyrights)
    else:
      copyrights.append([_GeneratedFilesDetector.NO_COPYRIGHT])
  return copyrights


def FindCopyrightViolations(input_api, root_dir, files_to_scan):
  """Looks for files that are not belong exlusively to the Chromium Authors.
  Args:
    input_api: InputAPI, as in presubmit scripts.
    root_dir: The root directory, to which all other paths are relative.
    files_to_scan: The list of file names to scan.
  Returns:
    The list of file names that contain non-Chromium copyrights.
  """
  copyrights = FindCopyrights(input_api, root_dir, files_to_scan)
  offending_files = []
  allowed_copyrights_re = input_api.re.compile(
    r'^(?:20[0-9][0-9](?:-20[0-9][0-9])? The Chromium Authors\. '
    'All rights reserved.*)$')
  for f, cs in itertools.izip(files_to_scan, copyrights):
    if cs[0] == _GeneratedFilesDetector.GENERATED_FILE or \
       cs[0] == _GeneratedFilesDetector.NO_COPYRIGHT:
      continue
    for c in cs:
      if not allowed_copyrights_re.match(c):
        offending_files.append(input_api.os_path.normpath(f))
        break
  return offending_files
