# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Various URI related helpers."""

from __future__ import print_function

import re
import urlparse


def _ExtractGobClAndSubpath(o):
  """Return a (CL, subpath) tuple if found in the URI parts, or None if not.

  Many forms are supported below.
  """
  path = o.path
  fragment = o.fragment

  # https://chromium-review.googlesource.com/#/662618
  # https://chromium-review.googlesource.com/#/662618/
  # https://chromium-review.googlesource.com/#/662618/////
  if path == '/':
    m = re.match(r'^/(?:c/)?([0-9]+)/*$', fragment)
    if m:
      return (m.group(1), '')

    # Any valid /c/ URI can also be in the fragment.
    if fragment.startswith('/c/'):
      # https://chrome-internal-review.googlesource.com/#/c/280497
      # https://chrome-internal-review.googlesource.com/#/c/280497/
      path = fragment
      fragment = ''
    else:
      return None

  # https://chromium-review.googlesource.com/662618
  # https://chromium-review.googlesource.com/662618/
  m = re.match(r'^/([0-9]+)/?$', path)
  if m:
    return (m.group(1), fragment)

  # https://chromium-review.googlesource.com/c/662618
  # https://chromium-review.googlesource.com/c/662618/
  # https://chromium-review.googlesource.com/c/662618/1/lib/gs.py
  m = re.match(r'^/c/([0-9]+)(/.*)?$', path)
  if m:
    return (m.group(1) + (m.group(2) or '').rstrip('/'), fragment)

  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618
  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618/
  # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/662618/1/lib/gs.py
  m = re.match(r'^/c/.*/\+/([0-9]+)(/.*)?$', path)
  if m:
    return (m.group(1) + (m.group(2) or '').rstrip('/'), fragment)

  return None


def _ShortenGob(o):
  """Shorten a Gerrit-on-Borg URI.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  parts = _ExtractGobClAndSubpath(o)
  if parts is None:
    return o

  # If we're using a Chromium GoB, use crrev.com.  Otherwise, we'll just
  # shorten the path but keep the original GoB host.
  if o.netloc.split('.')[0] in ('chromium-review', 'chrome-internal-review'):
    netloc = 'crrev.com'
    if o.netloc.startswith('chrome-internal'):
      sub = 'i'
    else:
      sub = 'c'
    path = '/%s/%s' % (sub, parts[0])
  else:
    netloc = o.netloc
    path = '/%s' % (parts[0],)

  return o._replace(scheme='https', netloc=netloc, path=path, fragment=parts[1])


def _ShortenCrosReview(o):
  """Shorten old review to new review hosts.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  m = re.match(r'^/(i/)?([0-9]+)', o.path)
  if m:
    subpart = 'i' if m.group(1) else 'c'
    o = o._replace(netloc='crrev.com',
                   path='/%s/%s' % (subpart, m.group(2)))

  return o._replace(scheme='https')


def _ShortenRietveld(o):
  """Shorten a rietveld URI.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  m = re.match(r'^/([0-9]+)', o.path)
  if m:
    o = o._replace(netloc='crrev.com',
                   path='/%s' % (m.group(1),))

  return o._replace(scheme='https')


def _ShortenBuganizer(o):
  """Shorten a buganizer URI.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  if o.path == '/issue':
    # http://b.corp.google.com/issue?id=123
    qs = urlparse.parse_qs(o.query)
    if 'id' in qs:
      o = o._replace(path='/%s' % (qs['id'][0],), query='')
  elif o.path.startswith('/issues/'):
    # http://b.corp.google.com/issues/123
    o = o._replace(path=o.path[len('/issues'):])

  return o._replace(scheme='http', netloc='b')


def _ShortenChromiumBug(o):
  """Shorten a Chromium bug URI.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  # https://bugs.chromium.org/p/chromium/issues/detail?id=123
  # https://bugs.chromium.org/p/google-breakpad/issues/list
  # These we don't actually shorten:
  # https://bugs.chromium.org/p/chromium/people
  m = re.match(r'/p/([^/]+)/issues(/detail)?', o.path)
  if m is None:
    return o

  if m.group(1) == 'chromium':
    path = ''
  else:
    path = '/%s' % (m.group(1),)

  if m.group(2):
    qs = urlparse.parse_qs(o.query)
    if 'id' in qs:
      path += '/%s' % (qs['id'][0],)
    o = o._replace(query='')

  return o._replace(scheme='https', netloc='crbug.com', path=path)


def _ShortenGutsTicket(o):
  """Shorten a Google GUTS ticket URI.

  Args:
    o: The named tuple from a urlparse.urlsplit call.

  Returns:
    A new named tuple that can be passed to urlparse.urlunsplit.
  """
  # https://gutsv3.corp.google.com/#ticket/123
  m = re.match(r'^ticket/([0-9]+)', o.fragment)
  if m:
    o = o._replace(path='/%s' % (m.group(1),), fragment='')

  return o._replace(scheme='http', netloc='t')


# Map sites to the shortener.  Use a tuple to keep rule ordering sane (even if
# we don't have rules atm relying on it).
_SHORTENERS = (
    (re.compile(r'^[a-z0-9-]+-review\.googlesource\.com$'), _ShortenGob),
    (re.compile(r'^crosreview\.com$'), _ShortenCrosReview),
    (re.compile(r'^codereview\.chromium\.org$'), _ShortenRietveld),
    (re.compile(r'^b\.corp\.google\.com$'), _ShortenBuganizer),
    (re.compile(r'^bugs\.chromium\.org$'), _ShortenChromiumBug),
    (re.compile(r'^gutsv\d\.corp\.google\.com$'), _ShortenGutsTicket),
)


def ShortenUri(uri, omit_scheme=False):
  """Attempt to shorten a URI for humans.

  If the URI can't be shortened, then we just return the original value.
  Thus this can be safely used as a pass-through for anything.

  Args:
    uri: Any valid URI.
    omit_scheme: Whether to include the scheme (e.g. http:// or https://).

  Returns:
    A (hopefully shorter) URI pointing to the same resource as |uri|.
  """
  o = urlparse.urlsplit(uri)

  # If the scheme & host are empty, assume it's because the URI we were given
  # lacked a http:// or https:// prefix, so blindly insert a http://.
  if not o.scheme and not o.netloc:
    o = urlparse.urlsplit('http://%s' % (uri,))

  for matcher, shortener in _SHORTENERS:
    if matcher.match(o.netloc):
      o = shortener(o)
      break
  else:
    return uri

  if omit_scheme:
    o = o._replace(scheme='')
    # Strip off the leading // due to blank scheme.
    return urlparse.urlunsplit(o)[2:]
  else:
    return urlparse.urlunsplit(o)
