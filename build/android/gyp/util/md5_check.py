# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import hashlib
import os
import re
import sys


# When set and a difference is detected, a diff of what changed is printed.
_PRINT_MD5_DIFFS = int(os.environ.get('PRINT_MD5_DIFFS', 0))

# Used to strip off temp dir prefix.
_TEMP_DIR_PATTERN = re.compile(r'^/tmp/.*?/')


def CallAndRecordIfStale(
    function, record_path=None, input_paths=None, input_strings=None,
    output_paths=None, force=False):
  """Calls function if outputs are stale.

  Outputs are considered stale if:
  - any output_paths are missing, or
  - the contents of any file within input_paths has changed, or
  - the contents of input_strings has changed.

  To debug which files are out-of-date, set the environment variable:
      PRINT_MD5_DIFFS=1

  Args:
    function: The function to call.
    record_path: Path to record metadata.
      Defaults to output_paths[0] + '.md5.stamp'
    input_paths: List of paths to calcualte an md5 sum on.
    input_strings: List of strings to record verbatim.
    output_paths: List of output paths.
    force: When True, function is always called.
  """
  assert record_path or output_paths
  input_paths = input_paths or []
  input_strings = input_strings or []
  output_paths = output_paths or []
  record_path = record_path or output_paths[0] + '.md5.stamp'
  md5_checker = _Md5Checker(
      record_path=record_path,
      input_paths=input_paths,
      input_strings=input_strings)

  missing_outputs = [x for x in output_paths if not os.path.exists(x)]
  is_stale = md5_checker.old_digest != md5_checker.new_digest

  if force or missing_outputs or is_stale:
    if _PRINT_MD5_DIFFS:
      print '=' * 80
      print 'Difference found in %s:' % record_path
      if missing_outputs:
        print 'Outputs do not exist:\n' + '\n'.join(missing_outputs)
      elif force:
        print 'force=True'
      else:
        print md5_checker.DescribeDifference()
      print '=' * 80
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

    for s in (str(s) for s in input_strings):
      outer_md5.update(s)
      extended_info.append(s)

    self.new_digest = outer_md5.hexdigest()
    self.new_extended_info = extended_info

    self.old_digest = ''
    self.old_extended_info = []
    if os.path.exists(self.record_path):
      with open(self.record_path, 'r') as old_record:
        self.old_extended_info = [line.strip() for line in old_record]
        if self.old_extended_info:
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
