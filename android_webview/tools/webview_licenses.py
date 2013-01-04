#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks third-party licenses for the purposes of the Android WebView build.

The Android tree includes a snapshot of Chromium in order to power the system
WebView.  This tool checks that all code uses open-source licenses compatible
with Android, and that we meet the requirements of those licenses. It can also
be used to generate an Android NOTICE file for the third-party code.

It makes use of src/tools/licenses.py and the README.chromium files on which
it depends. It also makes use of a data file, third_party_files_whitelist.txt,
which whitelists indicidual files which contain third-party code but which
aren't in a third-party directory with a README.chromium file.
"""

import optparse
import os
import re
import subprocess
import sys
import textwrap


REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))
import licenses

import known_issues

def GetIncompatibleDirectories():
  """Gets a list of third-party directories which use licenses incompatible
  with Android. This is used by the snapshot tool.
  Returns:
    A list of directories.
  """

  whitelist = [
    'Apache( Version)? 2(\.0)?',
    '(New )?BSD( [23]-Clause)?( with advertising clause)?',
    'L?GPL ?v?2(\.[01])?( or later)?',
    'MIT(/X11)?(-like)?',
    'MPL 1\.1 ?/ ?GPL 2(\.0)? ?/ ?LGPL 2\.1',
    'MPL 2(\.0)?',
    'Microsoft Limited Public License',
    'Microsoft Permissive License',
    'Public Domain',
    'SGI Free Software License B',
    'X11',
  ]
  regex = '^(%s)$' % '|'.join(whitelist)
  result = []
  for directory in _FindThirdPartyDirs():
    if directory in known_issues.KNOWN_ISSUES:
      result.append(directory)
      continue
    try:
      metadata = licenses.ParseDir(directory, REPOSITORY_ROOT,
                                   require_license_file=False)
    except licenses.LicenseError as e:
      print 'Got LicenseError while scanning ' + directory
      raise
    if metadata.get('License Android Compatible', 'no') == 'yes':
      continue
    license = re.split(' [Ll]icenses?$', metadata['License'])[0]
    tokens = [x.strip() for x in re.split(' and |,', license) if len(x) > 0]
    for token in tokens:
      if not re.match(regex, token, re.IGNORECASE):
        result.append(directory)
        break
  return result


def _CheckLicenseHeaders(directory_list, whitelisted_files):
  """Checks that all files which are not in a listed third-party directory,
  and which do not use the standard Chromium license, are whitelisted.
  Args:
    directory_list: The list of directories.
    whitelisted_files: The whitelist of files.
  Returns:
    True if all files with non-standard license headers are whitelisted and the
    whitelist contains no stale entries, otherwise false.
  """

  # Matches one of ...
  # - '[Cc]opyright', but not when followed by
  #   ' 20[0-9][0-9] The Chromium Authors.', with optional (c) and date range
  # - '([Cc]) (19|20)[0-9][0-9]', but not when preceeded by the word copyright,
  #   as this is handled above
  regex = '[Cc]opyright(?!( \(c\))? 20[0-9][0-9](-20[0-9][0-9])? ' \
          'The Chromium Authors\. All rights reserved\.)' \
          '|' \
          '(?<!(pyright |opyright))\([Cc]\) (19|20)[0-9][0-9]'

  args = ['grep',
          '-rPlI',
          '--exclude', '*.orig',
          '--exclude', '*.rej',
          '--exclude-dir', 'third_party',
          '--exclude-dir', 'out',
          '--exclude-dir', '.git',
          '--exclude-dir', '.svn',
          regex,
          '.']
  p = subprocess.Popen(args=args, cwd=REPOSITORY_ROOT, stdout=subprocess.PIPE)
  files = p.communicate()[0].splitlines()

  directory_list = directory_list[:]
  # Ignore these tools.
  directory_list.append('android_webview/tools/')
  # This is a build intermediate directory.
  directory_list.append('chrome/app/theme/google_chrome/')
  # This is tests directory, doesn't exist in the snapshot
  directory_list.append('content/test/data/')
  # This is a test output directory.
  directory_list.append('data/dom_perf/')
  # This is a test output directory.
  directory_list.append('data/page_cycler/')
  # 'Copyright' appears in strings.
  directory_list.append('chrome/app/resources/')
  # This is a Chrome on Linux reference build, doesn't exist in the snapshot
  directory_list.append('chrome/tools/test/reference_build/chrome_linux/')
  # Remoting internal tools, doesn't exist in the snapshot
  directory_list.append('remoting/appengine/')
  # Histogram tools, doesn't exist in the snapshot
  directory_list.append('tools/histograms/')
  # Arm sysroot tools, doesn't exist in the snapshot
  directory_list.append('arm-sysroot/')
  # Windows-only
  directory_list.append('tools/win/toolchain/7z/')

  # Exclude files under listed directories and some known offenders.
  offending_files = []
  for x in files:
    x = os.path.normpath(x)
    is_in_listed_directory = False
    for y in directory_list:
      if x.startswith(y):
        is_in_listed_directory = True
        break
    if not is_in_listed_directory:
      offending_files.append(x)

  all_files_valid = True
  unknown = set(offending_files) - set(whitelisted_files)
  if unknown:
    print 'The following files contain a third-party license but are not in ' \
          'a listed third-party directory and are not whitelisted. You must ' \
          'add the following files to the whitelist.\n%s' % \
          '\n'.join(sorted(unknown))
    all_files_valid = False

  stale = set(whitelisted_files) - set(offending_files)
  if stale:
    print 'The following files are whitelisted unnecessarily. You must ' \
          ' remove the following files from the whitelist.\n%s' % \
          '\n'.join(sorted(stale))
    all_files_valid = False

  return all_files_valid


def _ReadFile(path):
  """Reads a file from disk.
  Args:
    path: The path of the file to read, relative to the root of the repository.
  Returns:
    The contents of the file as a string.
  """

  return open(os.path.join(REPOSITORY_ROOT, path), 'rb').read()


def _FindThirdPartyDirs():
  """Gets the list of third-party directories.
  Returns:
    The list of third-party directories.
  """

  # Please don't add here paths that have problems with license files,
  # as they will end up included in Android WebView snapshot.
  # Instead, add them into known_issues.py.
  prune_paths = [
    # Placeholder directory, no third-party code.
    os.path.join('third_party', 'adobe'),
    # Apache 2.0 license. See
    # https://code.google.com/p/chromium/issues/detail?id=140478.
    os.path.join('third_party', 'bidichecker'),
    # Isn't checked out on clients
    os.path.join('third_party', 'gles2_conform'),
    # The llvm-build doesn't exist for non-clang builder
    os.path.join('third_party', 'llvm-build'),
    # Binaries doesn't apply to android
    os.path.join('third_party', 'widevine'),
  ]
  third_party_dirs = licenses.FindThirdPartyDirs(prune_paths, REPOSITORY_ROOT)
  return licenses.FilterDirsWithFiles(third_party_dirs, REPOSITORY_ROOT)


def _Scan():
  """Checks that license meta-data is present for all third-party code.
  Returns:
    Whether the check succeeded.
  """

  third_party_dirs = _FindThirdPartyDirs()

  # First, check designated third-party directories using src/tools/licenses.py.
  all_licenses_valid = True
  for path in sorted(third_party_dirs):
    try:
      licenses.ParseDir(path, REPOSITORY_ROOT)
    except licenses.LicenseError, e:
      if not (path in known_issues.KNOWN_ISSUES):
        print 'Got LicenseError "%s" while scanning %s' % (e, path)
        all_licenses_valid = False

  # Second, check for non-standard license text.
  files_data = _ReadFile(os.path.join('android_webview', 'tools',
                                      'third_party_files_whitelist.txt'))
  whitelisted_files = []
  for line in files_data.splitlines():
    match = re.match(r'([^#\s]+)', line)
    if match:
      whitelisted_files.append(match.group(1))
  return _CheckLicenseHeaders(third_party_dirs, whitelisted_files) \
      and all_licenses_valid


def GenerateNoticeFile():
  """Generates the contents of an Android NOTICE file for the third-party code.
  This is used by the snapshot tool.
  Returns:
    The contents of the NOTICE file.
  """

  third_party_dirs = _FindThirdPartyDirs()

  # Don't forget Chromium's LICENSE file
  content = [_ReadFile('LICENSE')]

  # We provide attribution for all third-party directories.
  # TODO(steveblock): Limit this to only code used by the WebView binary.
  for directory in third_party_dirs:
    metadata = licenses.ParseDir(directory, REPOSITORY_ROOT,
                                 require_license_file=False)
    license_file = metadata['License File']
    if license_file and license_file != licenses.NOT_SHIPPED:
      content.append(_ReadFile(license_file))

  return '\n'.join(content)


def main():
  class FormatterWithNewLines(optparse.IndentedHelpFormatter):
    def format_description(self, description):
      paras = description.split('\n')
      formatted_paras = [textwrap.fill(para, self.width) for para in paras]
      return '\n'.join(formatted_paras) + '\n'

  parser = optparse.OptionParser(formatter=FormatterWithNewLines(),
                                 usage='%prog [options]')
  parser.description = (__doc__ +
                       '\nCommands:\n' \
                       '  scan  Check licenses.\n' \
                       '  notice Generate Android NOTICE file on stdout')
  (options, args) = parser.parse_args()
  if len(args) != 1:
    parser.print_help()
    return 1

  if args[0] == 'scan':
    if _Scan():
      print 'OK!'
      return 0
    else:
      return 1
  elif args[0] == 'notice':
    print GenerateNoticeFile()
    return 0

  parser.print_help()
  return 1

if __name__ == '__main__':
  sys.exit(main())
