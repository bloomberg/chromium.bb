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

import glob
import imp
import optparse
import os
import re
import subprocess
import sys
import textwrap


REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

# Import third_party/PRESUBMIT.py via imp to avoid importing a random
# PRESUBMIT.py from $PATH, also make sure we don't generate a .pyc file.
sys.dont_write_bytecode = True
third_party = \
  imp.load_source('PRESUBMIT', \
                  os.path.join(REPOSITORY_ROOT, 'third_party', 'PRESUBMIT.py'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))
import licenses

import known_issues

class InputApi(object):
  def __init__(self):
    self.re = re

def GetIncompatibleDirectories():
  """Gets a list of third-party directories which use licenses incompatible
  with Android. This is used by the snapshot tool.
  Returns:
    A list of directories.
  """

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
    if metadata.get('License Android Compatible', 'no').upper() == 'YES':
      continue
    license = re.split(' [Ll]icenses?$', metadata['License'])[0]
    if not third_party.LicenseIsCompatibleWithAndroid(InputApi(), license):
      result.append(directory)
  return result

def GetUnknownIncompatibleDirectories():
  """Gets a list of third-party directories which use licenses incompatible
  with Android which are not present in the known_issues.py file.
  This is used by the AOSP bot.
  Returns:
    A list of directories.
  """
  incompatible_directories = frozenset(GetIncompatibleDirectories())
  known_incompatible = []
  for path, exclude_list in known_issues.KNOWN_INCOMPATIBLE.iteritems():
    for exclude in exclude_list:
      if glob.has_magic(exclude):
        exclude_dirname = os.path.dirname(exclude)
        if glob.has_magic(exclude_dirname):
          print ('Exclude path %s contains an unexpected glob expression,' \
                 ' skipping.' % exclude)
        exclude = exclude_dirname
      known_incompatible.append(os.path.normpath(os.path.join(path, exclude)))
  known_incompatible = frozenset(known_incompatible)
  return incompatible_directories.difference(known_incompatible)


class ScanResult(object):
  Ok, Warnings, Errors = range(3)

def _CheckLicenseHeaders(excluded_dirs_list, whitelisted_files):
  """Checks that all files which are not in a listed third-party directory,
  and which do not use the standard Chromium license, are whitelisted.
  Args:
    excluded_dirs_list: The list of directories to exclude from scanning.
    whitelisted_files: The whitelist of files.
  Returns:
    ScanResult.Ok if all files with non-standard license headers are whitelisted
    and the whitelist contains no stale entries;
    ScanResult.Warnings if there are stale entries;
    ScanResult.Errors if new non-whitelisted entries found.
  """

  excluded_dirs_list = [d for d in excluded_dirs_list if not 'third_party' in d]
  # Using a common pattern for third-partyies makes the ignore regexp shorter
  excluded_dirs_list.append('third_party')
  # VCS dirs
  excluded_dirs_list.append('.git')
  excluded_dirs_list.append('.svn')
  # Build output
  excluded_dirs_list.append('out/Debug')
  excluded_dirs_list.append('out/Release')
  # 'Copyright' appears in license agreements
  excluded_dirs_list.append('chrome/app/resources')
  # Quickoffice js files from internal src used on buildbots. crbug.com/350472.
  excluded_dirs_list.append('chrome/browser/resources/chromeos/quickoffice')
  # This is a test output directory
  excluded_dirs_list.append('chrome/tools/test/reference_build')
  # blink style copy right headers.
  excluded_dirs_list.append('content/shell/renderer/test_runner')
  # blink style copy right headers.
  excluded_dirs_list.append('content/shell/tools/plugin')
  # This is tests directory, doesn't exist in the snapshot
  excluded_dirs_list.append('content/test/data')
  # This is a tests directory that doesn't exist in the shipped product.
  excluded_dirs_list.append('gin/test')
  # This is a test output directory
  excluded_dirs_list.append('data/dom_perf')
  # This is a tests directory that doesn't exist in the shipped product.
  excluded_dirs_list.append('tools/perf/page_sets')
  excluded_dirs_list.append('tools/perf/page_sets/tough_animation_cases')
  # Histogram tools, doesn't exist in the snapshot
  excluded_dirs_list.append('tools/histograms')
  # Swarming tools, doesn't exist in the snapshot
  excluded_dirs_list.append('tools/swarming_client')
  # Arm sysroot tools, doesn't exist in the snapshot
  excluded_dirs_list.append('arm-sysroot')
  # Data is not part of open source chromium, but are included on some bots.
  excluded_dirs_list.append('data')
  # This is not part of open source chromium, but are included on some bots.
  excluded_dirs_list.append('skia/tools/clusterfuzz-data')

  args = ['android_webview/tools/find_copyrights.pl',
          '.'
          ] + excluded_dirs_list
  p = subprocess.Popen(args=args, cwd=REPOSITORY_ROOT, stdout=subprocess.PIPE)
  lines = p.communicate()[0].splitlines()

  offending_files = []
  allowed_copyrights = '^(?:\*No copyright\*' \
      '|20[0-9][0-9](?:-20[0-9][0-9])? The Chromium Authors\. ' \
      'All rights reserved.*)$'
  allowed_copyrights_re = re.compile(allowed_copyrights)
  for l in lines:
    entries = l.split('\t')
    if entries[1] == "GENERATED FILE":
      continue
    copyrights = entries[1].split(' / ')
    for c in copyrights:
      if c and not allowed_copyrights_re.match(c):
        offending_files.append(os.path.normpath(entries[0]))
        break

  unknown = set(offending_files) - set(whitelisted_files)
  if unknown:
    print 'The following files contain a third-party license but are not in ' \
          'a listed third-party directory and are not whitelisted. You must ' \
          'add the following files to the whitelist.\n%s' % \
          '\n'.join(sorted(unknown))

  stale = set(whitelisted_files) - set(offending_files)
  if stale:
    print 'The following files are whitelisted unnecessarily. You must ' \
          'remove the following files from the whitelist.\n%s' % \
          '\n'.join(sorted(stale))
  missing = [f for f in whitelisted_files if not os.path.exists(f)]
  if missing:
    print 'The following files are whitelisted, but do not exist.\n%s' % \
        '\n'.join(sorted(missing))

  if unknown:
    return ScanResult.Errors
  elif stale or missing:
    return ScanResult.Warnings
  else:
    return ScanResult.Ok


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
    # Temporary until we figure out how not to check out quickoffice on the
    # Android license check bot. Tracked in crbug.com/350472.
    os.path.join('chrome', 'browser', 'resources', 'chromeos', 'quickoffice'),
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
    # third_party directories in this tree aren't actually third party, but
    # provide a way to shadow experimental buildfiles into those directories.
    os.path.join('build', 'secondary'),
    # Not shipped, Chromium code
    os.path.join('tools', 'swarming_client'),
  ]
  third_party_dirs = licenses.FindThirdPartyDirs(prune_paths, REPOSITORY_ROOT)
  return licenses.FilterDirsWithFiles(third_party_dirs, REPOSITORY_ROOT)


def _Scan():
  """Checks that license meta-data is present for all third-party code and
     that all non third-party code doesn't contain external copyrighted code.
  Returns:
    ScanResult.Ok if everything is in order;
    ScanResult.Warnings if there are non-fatal problems (e.g. stale whitelist
      entries)
    ScanResult.Errors otherwise.
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
  licenses_check = _CheckLicenseHeaders(third_party_dirs, whitelisted_files)

  return licenses_check if all_licenses_valid else ScanResult.Errors


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
  for directory in sorted(third_party_dirs):
    metadata = licenses.ParseDir(directory, REPOSITORY_ROOT,
                                 require_license_file=False)
    license_file = metadata['License File']
    if license_file and license_file != licenses.NOT_SHIPPED:
      content.append(_ReadFile(license_file))

  return '\n'.join(content)


def _ProcessIncompatibleResult(incompatible_directories):
  if incompatible_directories:
    print ("Incompatibly licensed directories found:\n" +
           "\n".join(sorted(incompatible_directories)))
    return ScanResult.Errors
  return ScanResult.Ok

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
                       '  scan Check licenses.\n' \
                       '  notice Generate Android NOTICE file on stdout.\n' \
                       '  incompatible_directories Scan for incompatibly'
                       ' licensed directories.\n'
                       '  all_incompatible_directories Scan for incompatibly'
                       ' licensed directories (even those in'
                       ' known_issues.py).\n')
  (_, args) = parser.parse_args()
  if len(args) != 1:
    parser.print_help()
    return ScanResult.Errors

  if args[0] == 'scan':
    scan_result = _Scan()
    if scan_result == ScanResult.Ok:
      print 'OK!'
    return scan_result
  elif args[0] == 'notice':
    print GenerateNoticeFile()
    return ScanResult.Ok
  elif args[0] == 'incompatible_directories':
    return _ProcessIncompatibleResult(GetUnknownIncompatibleDirectories())
  elif args[0] == 'all_incompatible_directories':
    return _ProcessIncompatibleResult(GetIncompatibleDirectories())
  parser.print_help()
  return ScanResult.Errors

if __name__ == '__main__':
  sys.exit(main())
