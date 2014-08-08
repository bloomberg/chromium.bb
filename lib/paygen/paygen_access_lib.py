#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Paygen access time marker files.

This module facilitates the writing and reading of first and last access time
markers for paygen_build runs. Basically, these are two fixed named files that
are stored in each build's payloads directory and contain timestamps, which are
roughly the time when the markers were first and last updated, respectively.

This mechanism should work fine as far as handling race conditions when writing
a single marker; specifically, it ensures that the first access marker is
written only if new, and will always override the last marker.  This, however,
does not guarantee that the content of both files together will be consistent.
In particular, when two processes are updating the markers of the same build at
the same time, this may results in a first access timestamp that's more recent
than the last access one.  It is therefore advised that updating markers is
done while holding a GS lock on the build.

Also note that timestamps in markers are rounded down to the second, which
seems sufficient based on the granularity of paygen_build runs on a build.
Timestamp values are in UTC.
"""

import datetime
import os

import fixup_path
fixup_path.FixupPath()

from chromite.lib.paygen import gslib
from chromite.lib.paygen import gspaths
from chromite.lib.paygen import utils


# The format used for storing time in marker files. This is compatible with the
# format used for annotating log files uploaded by paygen.
_ACCESS_TIME_FMT = '%Y%m%d-%H%M%S-UTC'

# The names of the first and last access marker files.
_FIRST_MARKER_BASENAME = 'paygen-first-access.txt'
_LAST_MARKER_BASENAME = 'paygen-last-access.txt'


class Error(Exception):
  """Module exception base class."""


class MarkerWriteError(Error):
  """An error while writing a marker file."""


class MarkerReadError(Error):
  """An error while reading a marker file."""


def _GetMarkerUri(bucket, channel, board, version, basename):
  """Returns a GS path to a build's marker file of a given name.

  Args:
    bucket: The GS bucket with the build directory is located.
    channel: The channel that identifies the build.
    board: The board that identifies the build.
    version: The build's version.
    basename: The marker file's base name.

  Returns:
    A Google Storage URI of the marker file.
  """
  dirname = gspaths.ChromeosReleases.BuildPayloadsUri(channel, board, version,
                                                      bucket=bucket)
  return os.path.join(dirname, basename)


def _Markers(bucket, channel, board, version):
  """Returns the first and last marker file URIs for a given build."""
  return [_GetMarkerUri(bucket, channel, board, version, basename)
          for basename in (_FIRST_MARKER_BASENAME, _LAST_MARKER_BASENAME)]


def _UploadMarker(file_name, marker_uri, is_new):
  """Uploads a marker file to Google Storage.

  Args:
    file_name: A local file to upload as marker.
    marker_uri: A Google Storage URI.
    is_new: Whether the file must not previously exist.

  Raises:
    MarkerWriteError: When uploading failed, except when a file needs to be new
      and already exists.
  """
  try:
    gslib.Copy(file_name, marker_uri, generation=(0 if is_new else None))
  except gslib.CopyFail as e:
    if not (is_new and gslib.Exists(marker_uri)):
      raise MarkerWriteError('Failed to upload marker file %s: %s' %
                             (marker_uri, e))


def _ReadMarker(marker_uri):
  """Reads and parses the timestamp in a given marker file.

  Args:
    marker_uri: A Google Storage marker file URI.

  Returns:
    A datetime.datetime timestamp (UTC).

  Raises:
    MarkerReadError: If there was an error reading or parsing the timestamp.
  """
  try:
    return datetime.datetime.strptime(gslib.Cat(marker_uri), _ACCESS_TIME_FMT)
  except gslib.CatFail as e:
    raise MarkerReadError('Failed to read marker file %s: %s' %
                          (marker_uri, e))
  except ValueError as e:
    raise MarkerReadError('Failed to parse content of marker file %s: %s' %
                          (marker_uri, e))


def UpdateMarkers(channel, board, version, bucket=None):
  """Update first and last access markers for a build.

  The former is only written if not already present, the latter written
  unconditionally.

  Args:
    channel: The channel identifying the build.
    board: The board identifying the build.
    version: The build version.
    bucket: The GS bucket where the build directory is located.

  Raises:
    MarkerWriteError: If an error occurred while writing the markers.
  """
  first_marker_uri, last_marker_uri = _Markers(bucket, channel, board, version)
  timestamp = datetime.datetime.utcnow().strftime(_ACCESS_TIME_FMT)
  with utils.CreateTempFileWithContents(timestamp) as temp_file:
    _UploadMarker(temp_file.name, first_marker_uri, True)
    _UploadMarker(temp_file.name, last_marker_uri, False)


def GetMarkers(channel, board, version, bucket=None):
  """Reads the first and last access markers for a build.

  This returns the timestamps in both markers iff both read correctly.

  Args:
    channel: The channel identifying the build.
    board: The board identifying the build.
    version: The build version.
    bucket: The GS bucket where the build directory is located.

  Returns:
    A pair (first, last) containing datetime.datetime objects of corresponding
    access times.

  Raises:
    MarkerReadError: If an error occurred while reading/parsing the markers.
  """
  first_marker_uri, last_marker_uri = _Markers(bucket, channel, board, version)
  return _ReadMarker(first_marker_uri), _ReadMarker(last_marker_uri)


# IMPORTANT: This is a workaround for a Python bug having to do with strptime
# failing in a multithreaded setting due to a problem with lazy binding of the
# _strptime module within the time and (likely) datetime modules. It was
# suggested that an arbitrary call to strptime() prior to using it in threads
# fixes the problem. See http://bugs.python.org/issue7980 for details.
datetime.datetime.strptime('19700101', '%Y%m%d')
