# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for standard operations on URIs of different kinds."""

from __future__ import print_function

import re
import sys
import urllib

from chromite.lib.paygen import filelib
from chromite.lib.paygen import gslib


# This module allows files from different storage types to be handled
# in a common way, for supported operations.


PROTOCOL_GS = gslib.PROTOCOL
PROTOCOL_HTTP = 'http'
PROTOCOL_HTTPS = 'https'

PROTOCOLS = (PROTOCOL_GS,
             PROTOCOL_HTTP,
             PROTOCOL_HTTPS)

PROTOCOL_SEP = '://'

EXTRACT_PROTOCOL_RE = re.compile(r'^(\w+)%s' % PROTOCOL_SEP)

TYPE_GS = PROTOCOL_GS
TYPE_HTTP = PROTOCOL_HTTP
TYPE_HTTPS = PROTOCOL_HTTPS
TYPE_LOCAL = 'file'


class NotSupportedForType(RuntimeError):
  """Raised when operation is not supported for a particular file type"""

  def __init__(self, uri_type, extra_msg=None):
    # pylint: disable=protected-access
    function = sys._getframe(1).f_code.co_name
    msg = 'Function %s not supported for %s URIs' % (function, uri_type)
    if extra_msg:
      msg += ', ' + extra_msg

    RuntimeError.__init__(self, msg)


class NotSupportedForTypes(RuntimeError):
  """Raised when operation is not supported for all particular file type"""

  def __init__(self, extra_msg=None, *uri_types):
    # pylint: disable=protected-access
    function = sys._getframe(1).f_code.co_name
    msg = ('Function %s not supported for set of URIs with types: %s' %
           (function, ', '.join(uri_types)))
    if extra_msg:
      msg += ', ' + extra_msg

    RuntimeError.__init__(self, msg)


class NotSupportedBetweenTypes(RuntimeError):
  """Raised when operation is not supported between particular file types"""

  def __init__(self, uri_type1, uri_type2, extra_msg=None):
    # pylint: disable=protected-access
    function = sys._getframe(1).f_code.co_name
    msg = ('Function %s not supported between %s and %s URIs' %
           (function, uri_type1, uri_type2))
    if extra_msg:
      msg += ', ' + extra_msg

    RuntimeError.__init__(self, msg)


class MissingURLError(RuntimeError):
  """Raised when nothing exists at URL."""


def ExtractProtocol(uri):
  """Take a URI and return the protocol it is using, if any.

  Examples:
    'gs://some/path' ==> 'gs'
    'file:///some/path' ==> 'file'
    '/some/path' ==> None
    '/cns/some/colossus/path' ==> None

  Args:
    uri: The URI to get protocol from.

  Returns:
    Protocol string that is found, or None.
  """
  match = EXTRACT_PROTOCOL_RE.search(uri)
  if match:
    return match.group(1)

  return None


def GetUriType(uri):
  """Get the type of a URI.

  See the TYPE_* constants for examples.  This is mostly based
  on URI protocols, with Colossus and local files as exceptions.

  Args:
    uri: The URI to consider

  Returns:
    The URI type.
  """
  protocol = ExtractProtocol(uri)
  if protocol:
    return protocol

  return TYPE_LOCAL


def MD5Sum(uri):
  """Compute or retrieve MD5 sum of uri.

  Supported for: local files, GS files.

  Args:
    uri: The /unix/path or gs:// uri to compute the md5sum on.

  Returns:
    A string representing the md5sum of the file/uri passed in.
    None if we do not understand the uri passed in or cannot compute
    the md5sum.
  """

  uri_type = GetUriType(uri)

  if uri_type == TYPE_LOCAL:
    return filelib.MD5Sum(uri)
  elif uri_type == TYPE_GS:
    try:
      return gslib.MD5Sum(uri)
    except gslib.GSLibError:
      return None

  # Colossus does not have a command for getting MD5 sum.  We could
  # copy the file to local disk and calculate it, but it seems better
  # to explicitly say it is not supported.

  raise NotSupportedForType(uri_type)


class URLopener(urllib.FancyURLopener):
  """URLopener that will actually complain when download fails."""
  # The urllib.urlretrieve function, which seems like a good fit for this,
  # does not give access to error code.

  def http_error_default(self, *args, **kwargs):
    urllib.URLopener.http_error_default(self, *args, **kwargs)


def URLRetrieve(src_url, dest_path):
  """Download file from given URL to given local file path.

  Args:
    src_url: URL to download from.
    dest_path: Path to download to.

  Raises:
    MissingURLError if URL cannot be downloaded.
  """
  opener = URLopener()

  try:
    opener.retrieve(src_url, dest_path)
  except IOError as e:
    # If the domain is valid but download failed errno shows up as None.
    if e.errno is None:
      raise MissingURLError('Unable to download %s' % src_url)

    # If the domain is invalid the errno shows up as 'socket error', weirdly.
    try:
      int(e.errno)

      # This means there was some normal error writing to the dest_path.
      raise
    except ValueError:
      raise MissingURLError('Unable to download %s (bad domain?)' % src_url)


def Copy(src_uri, dest_uri):
  """Copy one uri to another.

  Args:
    src_uri: URI to copy from.
    dest_uri: Path to copy to.

  Raises:
    NotSupportedBetweenTypes if Cmp cannot be done between the two
      URIs provided.
  """
  uri_type1 = GetUriType(src_uri)
  uri_type2 = GetUriType(dest_uri)
  uri_types = set([uri_type1, uri_type2])

  if TYPE_GS in uri_types:
    # GS only supported between other GS files or local files.
    if len(uri_types) == 1 or TYPE_LOCAL in uri_types:
      return gslib.Copy(src_uri, dest_uri)

  if TYPE_LOCAL in uri_types and len(uri_types) == 1:
    return filelib.Copy(src_uri, dest_uri)

  if uri_type1 in (TYPE_HTTP, TYPE_HTTPS) and uri_type2 == TYPE_LOCAL:
    # Download file from URL.
    return URLRetrieve(src_uri, dest_uri)

  raise NotSupportedBetweenTypes(uri_type1, uri_type2)


def ListFiles(root_path, recurse=False, filepattern=None, sort=False):
  """Return list of file paths under given root path.

  Directories are intentionally excluded from results.  The root_path
  argument can be a local directory path, a Google storage directory URI,
  or a Colossus (/cns) directory path.

  Args:
    root_path: A local path, CNS path, or GS path to directory.
    recurse: Look for files in subdirectories, as well
    filepattern: glob pattern to match against basename of file
    sort: If True then do a default sort on paths

  Returns:
    List of paths to files that matched
  """
  uri_type = GetUriType(root_path)

  if TYPE_GS == uri_type:
    return gslib.ListFiles(root_path, recurse=recurse,
                           filepattern=filepattern, sort=sort)

  if TYPE_LOCAL == uri_type:
    return filelib.ListFiles(root_path, recurse=recurse,
                             filepattern=filepattern, sort=sort)

  raise NotSupportedForType(uri_type)
