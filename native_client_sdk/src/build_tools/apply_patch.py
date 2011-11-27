#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Utility for applying svn-diff-style patches to files.

This file contains a collection of classes for parsing svn-diff patch files
and applying these patches to files in a directory. The main API class is
ApplyPatch. It has two entry point: method Parse to parse a patch file and
method Apply to apply that patch to a base directory.

Most other classes are internal only. Each class corresponds to a structure in
the patch file, such as _Range, which corresponds to the range line identified
by the heading and traling '@@'.
"""

__author__ = 'gwink@google.com (Georges Winkenbach)'

import os
import re
import shutil
import sys
import tempfile

# Constants: regular expressions for parsing patch lines.
RE_HEADER_ORIGINAL_FILE = re.compile('---\s+([^\s]+)')
RE_HEADER_OUT_FILE = re.compile('\+\+\+\s+([^\s]+)')
RE_RANGE = re.compile('@@\s*-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s*@@')

# Constants: type of diff lines.
CONTEXTUAL_DIFF_LINE = 0
ADDED_DIFF_LINE = 1
DELETED_DIFF_LINE = 2
NOT_A_DIFF_LINE = 3


class Error(Exception):
  ''' Exceptions raised in this file.

  Attributes:
    error: a string that describes the error.
    context: a string that shows the context where the error occured.
  '''

  def __init__(self, context='', error='error'):
    ''' Init Error with context and error info. '''
    super(Error, self).__init__(context, error)
    self.error = error
    self.context = context

  def __str__(self):
    out = 'Error %s.' % self._error
    return out


class _Range(object):
  ''' Corresponds to a range info in the patch file.

  _Range encapsulates the source- and destination-file range information
  found in the patch file.

  Attributes:
    src_start_line: index to the first line in the source file.
    src_line_count: the number of lines in the source file.
    dest_start_line: index to the first line in the destination file.
    dest_line_count: the number of lines in the destination file.
  '''
  def __init__(self):
    ''' Init _Range with empty source and destination range info. '''
    self.src_start_line = 0
    self.src_line_count = 0
    self.dest_start_line = 0
    self.dest_line_count = 0

  def Parse(self, diff_lines):
    ''' Parse a range diff line.

    Parse the line at index 0 in diff_lines as a range-info line and set the
    range info accordingly. Raise an Error exception if the line is not a valid
    range line. If successful, the line at index 0 is removed from diff_lines.

    Arguments:
      diff_lines: a list of diff lines read from the patch file.
    '''
    match = re.match(RE_RANGE, diff_lines[0])
    if not match:
      raise Error(context=diff_lines[0], error='Bad range info')

    # Range in source file. The line count is optional and defaults to 1.
    self.src_start_line = int(match.group(1))
    self.src_line_count = 1
    if (match.group(2)):
      self.src_line_count = int(match.group(2))
    # Range in destination file. The line count is optional and defaults to 1.
    self.dest_start_line = int(match.group(3))
    self.dest_line_count = 1
    if (match.group(4)):
      self.dest_line_count = int(match.group(4))

    diff_lines.pop(0)


class _ChangeHunk(object):
  ''' A ChangeHunk is a range followed by a series of diff lines in the patch.

  Encapsulate the info represented by a range line and the subsequent diff
  lines in the patch file.

  Attributes:
    range: the range info, an instance of _Range
    lines: a list containing the diff lines that follow the range info in
        the patch file.
  '''

  def __init__(self):
    ''' Init an empty _ChangeHunk. '''
    self.range = None
    self.lines = []

  @staticmethod
  def _DiffLineType(line):
    ''' Helper function that determines the type of a diff line.

    Arguments:
      line: the diff line whose type must be determined.
    Return:
      One of CONTEXTUAL_DIFF_LINE, DELETED_DIFF_LINE, ADDED_DIFF_LINE or
          NOT_A_DIFF_LINE.
    '''
    # A contextual line starts with a space or can be an empty line.
    if line[0] in [' ', '\n', '\r']:
      return CONTEXTUAL_DIFF_LINE
    # A delete line starts with '-'; but '---' denotes a header line.
    elif line[0] == '-' and not line.startswith('---'):
      return DELETED_DIFF_LINE
    # A add line starts with '+'; but '+++' denotes a header line.
    elif line[0] == '+' and not line.startswith('+++'):
      return ADDED_DIFF_LINE
    # Anything else is not a diff line.
    else:
      return NOT_A_DIFF_LINE

  def _ParseLines(self, diff_lines):
    ''' Parse diff lines for a change-hunk.

    Parse the diff lines, update the change-hunk info accordingly and validate
    the hunk info. The first line in diff_lines should be the line that follows
    the range info in the patch file.. The subsequent lines should be diff
    lines. Raise an Error exception if the hunk info doesn't validate. Matched
    lines are removed from diff_lines.

    Arguments:
      diff_lines: a list of diff lines read from the patch file.
    '''
    src_line_count = 0
    dest_line_count = 0
    index = 0
    for index, line in enumerate(diff_lines):
      line_type = self._DiffLineType(line)
      if line_type == CONTEXTUAL_DIFF_LINE:
        # Contextual line (maybe an empty line).
        src_line_count += 1
        dest_line_count += 1
      elif line_type == DELETED_DIFF_LINE:
        # Deleted line.
        src_line_count += 1
      elif line_type == ADDED_DIFF_LINE:
        # Added line.
        dest_line_count += 1
      else:
        # Done.
        break;

      # Store each valid diff line into the local list.
      self.lines.append(line)

    # Remove lines that have been consumed from list.
    del diff_lines[0:index]

    # Validate the change hunk.
    if (src_line_count != self.range.src_line_count) or (dest_line_count !=
        self.range.dest_line_count):
      raise Error(context=self.lines[0],
                  error='Range info doesn\'t match lines that follow')

  def Parse(self, diff_lines):
    ''' Parse a change-hunk.

    Parse the range and subsequent diff lines for a change hunk. The first line
    in diff_lines should be a range line starting with @@. Raise an Error
    exception if the hunk info doesn't validate. Matched lines are removed from
    diff_lines.

    Arguments:
      diff_lines: a list of diff lines read from the patch file.
    '''
    self.range = _Range()
    self.range.Parse(diff_lines)
    self._ParseLines(diff_lines)

  def Apply(self, line_num, in_file, out_file):
    ''' Apply the change hunk to in_file.

    Apply the change hunk to in_file, outputing the result to out_file. Raise a
    Error exception if any inconsistency is detected.

    Arguments:
      line_num: the last line number read from in_file, or 0 if the file has not
          yet been read from.
      in_file: the input (source) file. Maybe None if the hunk only adds
          lines to a new file.
      out_file: the output (destination) file.
    Return:
      the line number of the last line read (consumed) from in_file.
    '''
    # If range starts at N, consume all lines from line_num to N-1. Don't read
    # line N yet; what we'll do with it depend on the first diff line. Each
    # line is written to the output file.
    while line_num < self.range.src_start_line - 1:
      out_file.write(in_file.readline())
      line_num = line_num + 1

    for diff_line in self.lines:
      diff_line_type = self._DiffLineType(diff_line)
      if diff_line_type == CONTEXTUAL_DIFF_LINE:
        # Remove the contextual-line marker, if present.
        if diff_line[0] == ' ':
          diff_line = diff_line[1:]
        # Contextual line should match the input line.
        line = in_file.readline()
        line_num = line_num + 1
        if not line == diff_line:
          raise Error(context=line, error='Contextual line mismatched')
        out_file.write(line)
      elif diff_line_type == DELETED_DIFF_LINE:
        # Deleted line: consume it from in_file, verify that it matches the
        # diff line (with marker removed), do not write it to out_file.
        line = in_file.readline()
        line_num = line_num + 1
        if not line == diff_line[1:]:
          raise Error(context=line, error='Deleted line mismatched')
      elif diff_line_type == ADDED_DIFF_LINE:
        # Added line: simply write it to out_file, without the add-line marker.
        diff_line = diff_line[1:]
        out_file.write(diff_line)
      else:
        # Not a valid diff line; this is an internal error.
        raise Error(context=diff_line, error='Unexpected diff line')

    return line_num


class _PatchHeader(object):
  ''' The header portion of a patch.

  Encapsulates a header, the two lines prefixed with '---' and '+++' before
  a series of change hunks.

  Attributes:
    in_file_name: the file name for the input (source) file.
    out_file_name: the file name for the output (new) file.
  '''
  def __init__(self):
    ''' Init _PatchHeader with no filenames. '''
    self.in_file_name = None
    self.out_file_name = None

  def Parse(self, diff_lines):
    ''' Parse header info.

    Parse the header information, the two lines starting with '---' and '+++'
    respectively. Gather the in (source) file name and out (new) file name
    locally. The first line in diff_lines should be a header line starting
    with '---'. Raise an Error exception if no header info is found. Matched
    lines are consumed (remove from diff_lines).

    Arguments:
      diff_lines: a list of diff lines read from the patch.
    '''
    match = self._MatchHeader(RE_HEADER_ORIGINAL_FILE, diff_lines)
    self.in_file_name = match.group(1)
    match = self._MatchHeader(RE_HEADER_OUT_FILE, diff_lines)
    self.out_file_name = match.group(1)

  def _MatchHeader(self, header_re, diff_lines):
    ''' Helper function to match a single header line using regular expression
        header_re.
    '''
    match = re.match(header_re, diff_lines[0])
    if not match:
      raise Error(context=diff_lines[0], error='Bad header')
    diff_lines.pop(0)
    return match


class _Patch(object):
  ''' A _Patch is the header and all the change hunks pertaining to a single
      file.

  Encapsulates all the information for a single-file patch, including the
  header and all the subsequent change hunks found in the patch file.

  Attributes:
    header: the header info, an instance of _PatchHeader.
    hunks: a list of _ChangeHunks.
  '''
  def __init__(self):
    ''' Init an empty _Patch. '''
    self.header = None
    self.hunks = []

  def _SrcFileIsOptional(self):
    ''' Helper function that returns true if it's ok for this patch to not have
        source file. (It's ok to not have a source file if the patch has a
        single hunk and that hunk only adds lines to the destination file.
    '''
    return len(self.hunks) == 1 and self.hunks[0].range.src_line_count == 0

  def Parse(self, diff_lines):
    ''' Parse a complete single-file patch.

    Parse a complete patch for a single file, updating the local patch info
    accordingly. Raise an Error exception if any inconsistency is detected.
    Patch lines from diff_lines are consumed as they are matched. The first line
    in diff_lines should be a header line starting with '---'.

    Arguments:
      diff_lines: a list diff lines read from the patch file.
    '''
    # Parse the patch header
    self.header = _PatchHeader()
    self.header.Parse(diff_lines)
    # The next line must be a range.
    if not diff_lines[0].startswith('@@'):
      raise Error(context=diff_lines[0], error='Bad range info')

    # Parse the range.
    while diff_lines:
      if diff_lines[0].startswith('@@'):
        hunk = _ChangeHunk()
        hunk.Parse(diff_lines)
        self.hunks.append(hunk)
      else:
        break

  def Apply(self, base_path):
    ''' Apply the patch to the directory base_path.

    Apply the single-file patch using base_path as the root directory.
    The source file name, extracted from the header, is appended to base_path
    to derive the full path to the file to be patched. The file is patched
    in-place. That is, if the patching is successful, the source file path
    will conatin the patched file when this method exits.

    Argument:
      base_path: path to the root directory.
    '''
    # Form an absolute path to the source file and verify that it exists if
    # it is required.
    in_path = os.path.abspath(
        os.path.join(base_path, self.header.in_file_name))
    if not os.path.exists(in_path) and not self._SrcFileIsOptional():
      raise Error(context=in_path, error='Original file doesn\'t exists')

    in_file = None
    out_file = None
    temp_out_path = None
    try:
      # Patch the input file into a temporary output file.
      if os.path.exists(in_path):
        in_file = open(in_path)
      with tempfile.NamedTemporaryFile(delete=False) as out_file:
        line_num = 0
        # Apply all the change hunks.
        for hunk in self.hunks:
          line_num = hunk.Apply(line_num, in_file, out_file)

        # We ran out of change hunks, but there may be lines left in the input
        # file. Copy them to the output file.
        if in_file:
          for line in in_file:
            out_file.write(line)

        # Close the input and output files; retain the temporary output file
        # path so we can rename it later.
        temp_out_path = out_file.name
        out_file.close()
        out_file = None
        if in_file:
          in_file.close()
          in_file = None

        # Rename the output file to the original input file name. Windows
        # forces us to delete the original file first. Linux is fussy about
        # renaming a temp file. So we copy and delete instead.
        if os.path.exists(in_path):
          os.remove(in_path)
        shutil.copy(temp_out_path, in_path)
        os.remove(temp_out_path)
    except:
      raise
    finally:
      if in_file: in_file.close()
      if out_file: out_file.close()
      if temp_out_path and os.path.exists(temp_out_path):
        os.remove(temp_out_path)


class _PatchSet(object):
  ''' All the patches from a patch file.

  Encapsulates all the patches found in a patch file.
  '''
  def __init__(self):
    self._patches = []

  def Parse(self, patch_file):
    ''' Parse all the patches in a patch file.

    Parse all the patches from a given patch file and store each patch info
    into list _patches. Raise an Error exception if an inconsistency is found.

    Argument:
      patch_file: a file descriptor for the patch file, ready for reading.
    '''
    diff_lines = patch_file.readlines()
    while diff_lines:
      # Look for the start of a header.
      if diff_lines[0].startswith('---'):
        patch = _Patch()
        patch.Parse(diff_lines)
        self._patches.append(patch)
      else:
        # Skip unrecognized lines.
        diff_lines.pop(0)

  def Apply(self, base_path):
    ''' Apply all the patches.

    Apply all the patches to files using base_path as the root directory.
    Raise an Error exception if an inconsistency is found. Files are patched
    in-place.

    Argument:
      base_path: path to the root directory in which to apply the patches.
    '''
    for patch in self._patches:
      patch.Apply(base_path)


class ApplyPatch(object):
  ''' Read a patch fileand apply it to a given base directory.

  ApplyPatch provides support for reading and parsing a patch file and
  applying the patches to files relative to a given base directory.
  '''

  def __init__(self):
    ''' Init ApplyPatch to empty. '''
    self._patch_set = None

  def Parse(self, patch_path):
    ''' Parse a patch file.

    Parse the patch file at path patch_path and store the patch info into
    _patch_set. Parsing-error messages are sent to stderr.

    Argument:
      patch_path: path to the patch file.

    Return:
      True if the patch file is successfully read and ready to apply and False
      otherwise.
    '''
    try:
      if not os.path.exists(patch_path):
        raise Error(context=patch_path, error="Patch file not found");
      with open(patch_path) as patch_file:
        self._patch_set = _PatchSet()
        self._patch_set.Parse(patch_file)

    except Error as (error):
      sys.stderr.write('Patch error: %s.\n' % error.error)
      sys.stderr.write('At line: %s\n' % error.context)
      return False

    return True

  def Apply(self, base_path):
    ''' Apply a patch to files.

    Apply an entire set of patches to files using base_path as the base
    directory. The files are patched in place. Patching-error message are output
    to stderr.

    Argument:
      base_path: path to the base directory in which to apply the patches.

    Return:
      True if all the patches are applied successfully and False otherwise.
    '''
    try:
      if not os.path.exists(base_path):
        raise Error(context=base_path, error="Invalid base path");
      if not os.path.isdir(base_path):
        raise Error(context=base_path, error="Base path is not a directory");
      self._patch_set.Apply(base_path)

    except Error as (error):
      sys.stderr.write('Processing error: %s.\n' % error.error)
      sys.stderr.write('>>> %s\n' % error.context)
      return False

    return True


def main(argv):
  apply_patch = ApplyPatch()
  if apply_patch.Parse(os.path.abspath(argv[0])):
    if apply_patch.Apply(os.path.abspath(argv[1])):
      sys.exit(0)

  sys.exit(1)


if __name__ == '__main__':
  if len(sys.argv) != 3:
    sys.stderr.write('Usage: %s patch-file base-dir\n' %
                     os.path.basename(sys.argv[0]))
    sys.exit(1)
  else:
    main(sys.argv[1:])
