# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is heavily based off of LUCI util.py. It's been adopted to remove
# AppEngine-ism.

"""Mixed bag of utilities."""

from __future__ import print_function

from datetime import datetime
from datetime import date
from datetime import time
from datetime import timedelta
from email import utils as email_utils
import binascii
import functools
import hashlib
import numbers
import os
import sys
import threading

from google.protobuf import timestamp_pb2
import six
from six.moves import urllib


THIS_DIR = os.path.dirname(os.path.abspath(__file__))

DATETIME_FORMAT = u'%Y-%m-%d %H:%M:%S'
DATE_FORMAT = u'%Y-%m-%d'
VALID_DATETIME_FORMATS = ('%Y-%m-%d', '%Y-%m-%d %H:%M', '%Y-%m-%d %H:%M:%S')

# UTC datetime corresponding to zero Unix timestamp.
EPOCH = datetime.utcfromtimestamp(0)

# Module to run task queue tasks on by default. Used by get_task_queue_host
# function. Can be changed by 'set_task_queue_module' function.
_task_queue_module = 'backend'


def should_disable_ui_routes():
  return os.environ.get('LUCI_DISABLE_UI_ROUTES', '0') == '1'


def get_request_as_int(request, key, default, min_value, max_value):
  """Returns a request value as int."""
  value = request.params.get(key, '')
  try:
    value = int(value)
  except ValueError:
    return default
  return min(max_value, max(min_value, value))


def parse_datetime(text):
  """Converts text to datetime.datetime instance or None."""
  for f in VALID_DATETIME_FORMATS:
    try:
      return datetime.strptime(text, f)
    except ValueError:
      continue
  return None


def parse_rfc3339_datetime(value):
  """Parses RFC 3339 datetime string (as used in Timestamp proto JSON encoding).

  Keeps only microsecond precision (dropping nanoseconds).

  Examples of the input:
    2017-08-17T04:21:32.722952943Z
    1972-01-01T10:00:20.021-05:00

  Returns:
    datetime.datetime in UTC (regardless of timezone of the original string).

  Raises:
    ValueError on errors.
  """
  # Adapted from protobuf/internal/well_known_types.py Timestamp.FromJsonString.
  # We can't use the original, since it's marked as internal. Also instantiating
  # proto messages here to parse a string would been odd.
  timezone_offset = value.find('Z')
  if timezone_offset == -1:
    timezone_offset = value.find('+')
  if timezone_offset == -1:
    timezone_offset = value.rfind('-')
  if timezone_offset == -1:
    raise ValueError('Failed to parse timestamp: missing valid timezone offset')
  time_value = value[0:timezone_offset]
  # Parse datetime and nanos.
  point_position = time_value.find('.')
  if point_position == -1:
    second_value = time_value
    nano_value = ''
  else:
    second_value = time_value[:point_position]
    nano_value = time_value[point_position + 1:]
  date_object = datetime.strptime(second_value, '%Y-%m-%dT%H:%M:%S')
  td = date_object - EPOCH
  seconds = td.seconds + td.days * 86400
  if len(nano_value) > 9:
    raise ValueError(
        'Failed to parse timestamp: nanos %r more than 9 fractional digits' %
        nano_value)
  if nano_value:
    nanos = round(float('0.' + nano_value) * 1e9)
  else:
    nanos = 0
  # Parse timezone offsets.
  if value[timezone_offset] == 'Z':
    if len(value) != timezone_offset + 1:
      raise ValueError(
          'Failed to parse timestamp: invalid trailing data %r' % value)
  else:
    timezone = value[timezone_offset:]
    pos = timezone.find(':')
    if pos == -1:
      raise ValueError('Invalid timezone offset value: %r' % timezone)
    if timezone[0] == '+':
      seconds -= (int(timezone[1:pos]) * 60 + int(timezone[pos + 1:])) * 60
    else:
      seconds += (int(timezone[1:pos]) * 60 + int(timezone[pos + 1:])) * 60
  return timestamp_to_datetime(int(seconds) * 1e6 + int(nanos) // 1e3)

def TimestampToDatetime(input_time):
  """Converts seconds in google.protobuf.Timestamp to readable format.

  Args:
    input_time: google.protobuf.Timestamp instance.

  Returns:
    datetime.datetime instance corresponding to input_time.
  """
  if input_time and input_time.seconds != 0:
    return datetime.fromtimestamp(input_time.seconds)
  else:
    return None

def DatetimeToTimestamp(input_date, end_of_day=False):
  """Converts datetime.date object to Timestamp instance.

  Args:
    input_date: datetime.date instance to be converted.
    end_of_day: Boolean indicating whether the Timestamp correponds to the
        end of the date in input_date.

  Returns:
    A Timestamp instance corresponding to the specific date.
  """
  assert isinstance(input_date, date)
  if end_of_day:
    datetime_instance = datetime.combine(input_date, time(23, 59))
  else:
    datetime_instance = datetime.combine(input_date, time(0, 0))
  micros = datetime_to_timestamp(datetime_instance)
  return timestamp_pb2.Timestamp(seconds=micros // (1000 * 1000))

def constant_time_equals(a, b):
  """Compares two strings in constant time regardless of theirs content."""
  if len(a) != len(b):
    return False
  result = 0
  for x, y in zip(a, b):
    result |= ord(x) ^ ord(y)
  return result == 0


def to_units(number):
  """Convert a string to numbers."""
  UNITS = ('', 'k', 'm', 'g', 't', 'p', 'e', 'z', 'y')
  unit = 0
  while number >= 1024.:
    unit += 1
    number = number // 1024.
    if unit == len(UNITS) - 1:
      break
  if unit:
    return '%.2f%s' % (number, UNITS[unit])
  return '%d' % number


def validate_root_service_url(url):
  """Raises ValueError if the URL doesn't look like https://<host>."""
  schemes = ('https', 'http')
  parsed = urllib.parse.urlparse(url)
  if parsed.scheme not in schemes:
    raise ValueError('unsupported protocol %r' % str(parsed.scheme))
  if not parsed.netloc:
    raise ValueError('missing hostname')
  stripped = urllib.parse.urlunparse((parsed[0], parsed[1], '', '', '', ''))
  if stripped != url:
    raise ValueError('expecting root host URL, e.g. %r)' % str(stripped))


### Time


def utcnow():
  """Returns datetime.utcnow(), used for testing.

  Use this function so it can be mocked everywhere.
  """
  return datetime.utcnow()


def time_time():
  """Returns the equivalent of time.time() as mocked if applicable."""
  return (utcnow() - EPOCH).total_seconds()


def milliseconds_since_epoch(now):
  """Returns the number of milliseconds since unix epoch as an int."""
  now = now or utcnow()
  return int(round((now - EPOCH).total_seconds() * 1000.))


def datetime_to_rfc2822(dt):
  """datetime -> string value for Last-Modified header as defined by RFC2822."""
  if not isinstance(dt, datetime):
    raise TypeError(
        'Expecting datetime object, got %s instead' % type(dt).__name__)
  assert dt.tzinfo is None, 'Expecting UTC timestamp: %s' % dt
  return email_utils.formatdate(datetime_to_timestamp(dt) // 1000000)


def datetime_to_timestamp(value):
  """Converts UTC datetime to integer timestamp in microseconds since epoch."""
  if not isinstance(value, datetime):
    raise ValueError(
        'Expecting datetime object, got %s instead' % type(value).__name__)
  if value.tzinfo is not None:
    raise ValueError('Only UTC datetime is supported')
  dt = value - EPOCH
  return dt.microseconds + 1000 * 1000 * (dt.seconds + 24 * 3600 * dt.days)


def timestamp_to_datetime(value):
  """Converts integer timestamp in microseconds since epoch to UTC datetime."""
  if not isinstance(value, numbers.Real):
    raise ValueError(
        'Expecting a number, got %s instead' % type(value).__name__)
  return EPOCH + timedelta(microseconds=value)


### Cache


class _Cache(object):
  """Holds state of a cache for cache_with_expiration and cache decorators.

  May call func more than once.
  Thread- and NDB tasklet-safe.
  """

  def __init__(self, func, expiration_sec):
    self.func = func
    self.expiration_sec = expiration_sec
    self.lock = threading.Lock()
    self.value = None
    self.value_is_set = False
    self.expires = None

  def get_value(self):
    """Returns a cached value refreshing it if it has expired."""
    with self.lock:
      if self.value_is_set and (not self.expires or time_time() < self.expires):
        return self.value

    new_value = self.func()

    with self.lock:
      self.value = new_value
      self.value_is_set = True
      if self.expiration_sec:
        self.expires = time_time() + self.expiration_sec

    return self.value

  def clear(self):
    """Clears stored cached value."""
    with self.lock:
      self.value = None
      self.value_is_set = False
      self.expires = None

  def get_wrapper(self):
    """Returns a callable object that can be used in place of |func|.

    It's basically self.get_value, updated by functools.wraps to look more like
    original function.
    """
    # functools.wraps doesn't like 'instancemethod', use lambda as a proxy.
    # pylint: disable=W0108
    wrapper = functools.wraps(self.func)(lambda: self.get_value())
    wrapper.__parent_cache__ = self
    return wrapper


def cache(func):
  """Decorator that implements permanent cache of a zero-parameter function."""
  return _Cache(func, None).get_wrapper()


def cache_with_expiration(expiration_sec):
  """Decorator that implements in-memory cache for a zero-parameter function."""

  def decorator(func):
    return _Cache(func, expiration_sec).get_wrapper()

  return decorator


def clear_cache(func):
  """Given a function decorated with @cache, resets cached value."""
  func.__parent_cache__.clear()


## General


def get_token_fingerprint(blob):
  """Given a blob with a token returns first 16 bytes of its SHA256 as hex.

  It can be used to identify this particular token in logs without revealing it.
  """
  assert isinstance(blob, six.string_types)
  if isinstance(blob, six.text_type):
    blob = blob.encode('ascii', 'ignore')
  return binascii.hexlify(hashlib.sha256(blob).digest()[:16])


## Hacks


def fix_protobuf_package():
  """Modifies 'google' package to include path to 'google.protobuf' package.

  Prefer our own proto package on the server. Note that this functions is not
  used on the Swarming bot nor any other client.
  """
  # google.__path__[0] will be google_appengine/google.
  import google
  if len(google.__path__) > 1:
    return

  # We do not mind what 'google' get used, inject protobuf in there.
  path = os.path.join(THIS_DIR, 'third_party', 'protobuf', 'google')
  google.__path__.append(path)

  # six is needed for oauth2client and webtest (local testing).
  six_path = os.path.join(THIS_DIR, 'third_party', 'six')
  if six_path not in sys.path:
    sys.path.insert(0, six_path)


def import_jinja2():
  """Remove any existing jinja2 package and add ours."""
  for i in sys.path[:]:
    if os.path.basename(i) == 'jinja2':
      sys.path.remove(i)
  sys.path.append(os.path.join(THIS_DIR, 'third_party'))
