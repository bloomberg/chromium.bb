# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import hashlib
import os
import re
import sys

from util import build_utils

if build_utils.COLORAMA_ROOT not in sys.path:
  sys.path.append(build_utils.COLORAMA_ROOT)
import colorama


# When set and a difference is detected, a diff of what changed is printed.
_PRINT_MD5_DIFFS = int(os.environ.get('PRINT_MD5_DIFFS', 0))

# Used to strip off temp dir prefix.
_TEMP_DIR_PATTERN = re.compile(r'^/tmp/.*?/')


def CallAndRecordIfStale(
    function, record_path=None, input_paths=None, input_strings=None,
    force=False):
  """Calls function if the md5sum of the input paths/strings has changed.

  The md5sum of the inputs is compared with the one stored in record_path. If
  this has changed (or the record doesn't exist), function will be called and
  the new md5sum will be recorded.

  If force is True, the function will be called regardless of whether the
  md5sum is out of date.
  """
  if not input_paths:
    input_paths = []
  if not input_strings:
    input_strings = []
  md5_checker = _Md5Checker(
      record_path=record_path,
      input_paths=input_paths,
      input_strings=input_strings)

  is_stale = md5_checker.old_digest != md5_checker.new_digest
  if force or is_stale:
    if is_stale and _PRINT_MD5_DIFFS:
      print '%sDifference found in %s:%s' % (
          colorama.Fore.YELLOW, record_path, colorama.Fore.RESET)
      print md5_checker.DescribeDifference()
    function()
    md5_checker.Write()


def _UpdateMd5ForFile(md5, path, block_size=2**16):
  with open(path, 'rb') as infile:
    while True:
      data = infile.read(block_size)
      if not data:
        break
      md5.update(data)


def _UpdateMd5ForDirectory(md5, dir_path):
  for root, _, files in os.walk(dir_path):
    for f in files:
      _UpdateMd5ForFile(md5, os.path.join(root, f))


def _UpdateMd5ForPath(md5, path):
  if os.path.isdir(path):
    _UpdateMd5ForDirectory(md5, path)
  else:
    _UpdateMd5ForFile(md5, path)


def _TrimPathPrefix(path):
  """Attempts to remove temp dir prefix from the path.

  Use this only for extended_info (not for the actual md5).
  """
  return _TEMP_DIR_PATTERN.sub('{TMP}', path)


class _Md5Checker(object):
  def __init__(self, record_path=None, input_paths=None, input_strings=None):
    if not input_paths:
      input_paths = []
    if not input_strings:
      input_strings = []

    assert record_path.endswith('.stamp'), (
        'record paths must end in \'.stamp\' so that they are easy to find '
        'and delete')

    self.record_path = record_path

    extended_info = []
    outer_md5 = hashlib.md5()
    for i in sorted(input_paths):
      inner_md5 = hashlib.md5()
      _UpdateMd5ForPath(inner_md5, i)
      i = _TrimPathPrefix(i)
      extended_info.append(i + '=' + inner_md5.hexdigest())
      # Include the digest in the overall diff, but not the path
      outer_md5.update(inner_md5.hexdigest())

    for s in input_strings:
      outer_md5.update(s)
      extended_info.append(s)

    self.new_digest = outer_md5.hexdigest()
    self.new_extended_info = extended_info

    self.old_digest = ''
    self.old_extended_info = []
    if os.path.exists(self.record_path):
      with open(self.record_path, 'r') as old_record:
        self.old_extended_info = [line.strip() for line in old_record]
        self.old_digest = self.old_extended_info.pop(0)

  def Write(self):
    with open(self.record_path, 'w') as new_record:
      new_record.write(self.new_digest)
      new_record.write('\n' + '\n'.join(self.new_extended_info) + '\n')

  def DescribeDifference(self):
    if self.old_digest == self.new_digest:
      return "There's no difference."
    if not self.old_digest:
      return 'Previous stamp file not found.'
    if not self.old_extended_info:
      return 'Previous stamp file lacks extended info.'
    diff = difflib.unified_diff(self.old_extended_info, self.new_extended_info)
    return '\n'.join(diff)
