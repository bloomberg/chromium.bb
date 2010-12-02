# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import subprocess
import os

try:
  from internal import internal_presubmit
except ImportError:
  internal_presubmit = None


SOURCE_FILE_EXTENSIONS = [
        '.c', '.cc', '.cpp', '.h', '.m', '.mm', '.py', '.mk', '.am', '.json',
        '.gyp', '.gypi'
        ]

EXCLUDED_PATHS = []

# Finds what seem to be definitions of DllRegisterServer.
DLL_REGISTER_SERVER_RE = re.compile('\s*STDAPI\s+DllRegisterServer\s*\(')

# Matches a Tracker story URL
story_url_re = re.compile('https?://tracker.+/[0-9]+')

# Matches filenames in which we allow tabs
tabs_ok_re = re.compile('.*\.(vcproj|vsprops|sln)$')

# Matches filenames of source files
source_files_re = re.compile('.*\.(cc|h|py|js)$')


# The top-level source directory.
_SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def CheckChange(input_api, output_api, committing, is_chrome_frame=False):
  results = []

  do_not_submit_errors = input_api.canned_checks.CheckDoNotSubmit(input_api,
                                                                  output_api)
  if committing:
    results += do_not_submit_errors
  elif do_not_submit_errors:
    results += [output_api.PresubmitNotifyResult(
        'There is a DO-NOT-SUBMIT issue')]

  results += CheckChangeHasNoTabs(input_api, output_api)
  results += CheckLongLines(input_api, output_api)
  results += CheckHasStoryOrBug(input_api, output_api)
  results += LocalChecks(input_api, output_api)
  results += WarnOnAtlSmartPointers(input_api, output_api)
  if not is_chrome_frame:
    results += CheckNoDllRegisterServer(input_api, output_api)
    results += CheckUnittestsRan(input_api, output_api, committing)
    if internal_presubmit:
      results += internal_presubmit.InternalChecks(input_api, output_api,
                                                   committing)
  return results


def CheckHasStoryOrBug(input_api, output_api):
  """We require either BUG= to be present and non-empty.  For
  a completely trivial change, use BUG=none.
  """
  if not ('BUG' in input_api.change.tags):
    return [output_api.PresubmitError('A BUG= tag is required. For trivial '
                                      'changes you can use BUG=none.')]
  if ('BUG' in input_api.change.tags and
      len(input_api.change.tags['BUG']) == 0):
    return [output_api.PresubmitError('A non-empty BUG= is required. For '
                                      'trivial changes you can use BUG=none.')]
  return []


def CheckChangeHasNoTabs(input_api, output_api):
  """Slightly modified version of the canned check with the same name.

  This version ignores certain file types in which we allow tabs.
  """
  for f, line_num, line in input_api.RightHandSideLines():
    if tabs_ok_re.match(f.LocalPath()):
      continue
    if '\t' in line:
      return [output_api.PresubmitError(
          "Found a tab character in %s, line %s" %
          (f.LocalPath(), line_num))]
  return []


def CheckLongLines(input_api, output_api, maxlen=80):
  """Checks that there aren't any lines longer than maxlen characters in any of
  the text files to be submitted.
  """
  basename = input_api.basename

  bad = []
  for f, line_num, line in input_api.RightHandSideLines():
    if not source_files_re.match(f.LocalPath()):
      continue
    if line.find('http://') != -1:
      # Exemption for long URLs
      continue
    if line.endswith('\n'):
      line = line[:-1]
    if len(line) > maxlen:
      bad.append(
          '%s, line %s, %s chars' %
          (basename(f.LocalPath()), line_num, len(line)))
      if len(bad) == 5:  # Just show the first 5 errors.
        break

  if bad:
    msg = "Found lines longer than %s characters (first 5 shown)." % maxlen
    return [output_api.PresubmitPromptWarning(msg, items=bad)]
  else:
    return []


_UNITTEST_MESSAGE = '''\
You must build and run the CEEE smoke tests before submitting. To clear this
error, run the script "smoke_test.bat" in the CEEE directory.
'''


def CheckUnittestsRan(input_api, output_api, committing):
  '''Checks that the unittests success file is newer than any modified file'''
  # But only if there were IE files modified, since we only have unit tests
  # for CEEE IE.
  files = []
  ie_paths_re = re.compile('ceee[\\\\/](ie|common)[\\\\/]')
  for f in input_api.AffectedFiles(include_deletes = False):
    path = f.LocalPath()
    if (ie_paths_re.match(path)):
      files.append(f)

  if not files:
    return []

  def MakeResult(message, modified_files=[]):
    if committing:
      return output_api.PresubmitError(message, modified_files)
    else:
      return output_api.PresubmitNotifyResult(message, modified_files)
  os_path = input_api.os_path

  success_files = [
      os_path.join(input_api.PresubmitLocalPath(),
                    '../chrome/Debug/ceee.success'),
      os_path.join(input_api.PresubmitLocalPath(),
                   '../chrome/Release/ceee.success')]

  if (not os_path.exists(success_files[0]) or
      not os_path.exists(success_files[1])):
    return [MakeResult(_UNITTEST_MESSAGE)]

  success_time = min(os.stat(success_files[0]).st_mtime,
                     os.stat(success_files[1]).st_mtime)
  modified_files = []
  for f in modified_files:
    file_time = os.stat(f.AbsoluteLocalPath()).st_mtime
    if file_time > success_time:
      modified_files.append(f.LocalPath())

  result = []
  if modified_files:
    result.append(MakeResult('These files have been modified since Debug and/or'
                             ' Release unittests were built.', modified_files))

  return result


def CheckNoDllRegisterServer(input_api, output_api):
  for f, line_num, line in input_api.RightHandSideLines():
    if DLL_REGISTER_SERVER_RE.search(line):
      file_name = os.path.basename(f.LocalPath())
      if file_name not in ['install_utils.h', 'install_utils_unittest.cc']:
        return [output_api.PresubmitError(
            '%s contains a definition of DllRegisterServer at line %s.\n'
            'Please search for CEEE_DEFINE_DLL_REGISTER_SERVER.' %
            (f.LocalPath(), line_num))]
  return []


def WarnOnAtlSmartPointers(input_api, output_api):
  smart_pointer_re = re.compile(r'\bCCom(Ptr|BSTR|Variant)\b')
  bad_files = []
  for f in input_api.AffectedFiles(include_deletes=False):
    contents = input_api.ReadFile(f, 'r')
    if smart_pointer_re.search(contents):
      bad_files.append(f.LocalPath())
  if bad_files:
    return [output_api.PresubmitPromptWarning(
        'The following files use CComPtr, CComBSTR and/or CComVariant.\n'
        'Please consider switching to ScopedComPtr, ScopedBstr and/or\n'
        'ScopedVariant as per team policy. (NOTE: Will soon be an error.)',
        items=bad_files)]
  else:
    return []


def LocalChecks(input_api, output_api, max_cols=80):
  """Reports an error if for any source file in SOURCE_FILE_EXTENSIONS:
     - uses CR (or CRLF)
     - contains a TAB
     - has a line that ends with whitespace
     - contains a line >|max_cols| cols unless |max_cols| is 0.
     - File does not end in a newline, or ends in more than one.

  Note that the whole file is checked, not only the changes.
  """
  C_SOURCE_FILE_EXTENSIONS = ('.c', '.cc', '.cpp', '.h', '.inl')
  cr_files = []
  eof_files = []
  results = []
  excluded_paths = [input_api.re.compile(x) for x in EXCLUDED_PATHS]
  files = input_api.AffectedFiles(include_deletes=False)
  for f in files:
    path = f.LocalPath()
    root, ext = input_api.os_path.splitext(path)
    # Look for unsupported extensions.
    if not ext in SOURCE_FILE_EXTENSIONS:
      continue
    # Look for excluded paths.
    found = False
    for item in excluded_paths:
      if item.match(path):
        found = True
        break
    if found:
      continue

    # Need to read the file ourselves since AffectedFile.NewContents()
    # will normalize line endings.
    contents = input_api.ReadFile(f, 'rb')
    if '\r' in contents:
      cr_files.append(path)

    # Check that the file ends in one and only one newline character.
    if len(contents) > 0 and (contents[-1:] != "\n" or contents[-2:-1] == "\n"):
      eof_files.append(path)

    local_errors = []
    # Remove end of line character.
    lines = contents.splitlines()
    line_num = 1
    for line in lines:
      if line.endswith(' '):
        local_errors.append(output_api.PresubmitError(
            '%s, line %s ends with whitespaces.' %
            (path, line_num)))
      # Accept lines with http://, https:// and C #define/#pragma/#include to
      # exceed the max_cols rule.
      if (max_cols and
          len(line) > max_cols and
          not 'http://' in line and
          not 'https://' in line and
          not (line[0] == '#' and ext in C_SOURCE_FILE_EXTENSIONS)):
        local_errors.append(output_api.PresubmitError(
            '%s, line %s has %s chars, please reduce to %d chars.' %
            (path, line_num, len(line), max_cols)))
      if '\t' in line:
        local_errors.append(output_api.PresubmitError(
            "%s, line %s contains a tab character." %
            (path, line_num)))
      line_num += 1
      # Just show the first 5 errors.
      if len(local_errors) == 6:
        local_errors.pop()
        local_errors.append(output_api.PresubmitError("... and more."))
        break
    results.extend(local_errors)

  if cr_files:
    results.append(output_api.PresubmitError(
        'Found CR (or CRLF) line ending in these files, please use only LF:',
        items=cr_files))
  if eof_files:
    results.append(output_api.PresubmitError(
        'These files should end in one (and only one) newline character:',
        items=eof_files))
  return results
