# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import contextlib
import os
import platform
import shutil
import sys
import tempfile


ROOT = os.path.dirname(os.path.abspath(__file__))
WHEELHOUSE = os.path.join(ROOT, 'wheelhouse')

BUCKET = 'chrome-python-wheelhouse'
STORAGE_URL = 'https://www.googleapis.com/storage/v1/b/{}/o'.format(BUCKET)
OBJECT_URL = 'https://storage.googleapis.com/{}/{{}}#md5={{}}'.format(BUCKET)
LOCAL_OBJECT_URL = 'file://{}'

LOCAL_STORAGE_PATH = os.path.join(ROOT, 'wheelhouse_cache')

SOURCE_URL = 'gs://{}/sources/{{}}'.format(BUCKET)
WHEELS_URL = 'gs://{}/wheels/'.format(BUCKET)


class DepsConflictException(Exception):
  def __init__(self, name):
    super(DepsConflictException, self).__init__(
        'Package \'%s\' is defined twice in deps.pyl' % name)


def platform_tag():
  if sys.platform.startswith('linux'):
    return '_{0}_{1}'.format(*platform.linux_distribution())
  return ''


def print_deps(deps, indent=1, with_implicit=True):
  for dep, entry in deps.iteritems():
    if not with_implicit and entry.get('implicit'):
      continue
    print '  ' * indent + '%s: %r' % (dep, entry)
  print


@contextlib.contextmanager
def tempdir(*args, **kwargs):
  tdir = None
  try:
    tdir = tempfile.mkdtemp(*args, **kwargs)
    yield tdir
  finally:
    if tdir:
      shutil.rmtree(tdir, ignore_errors=True)


@contextlib.contextmanager
def tempname(*args, **kwargs):
  tmp = None
  try:
    tmp = tempfile.mktemp(*args, **kwargs)
    yield tmp
  finally:
    if tmp:
      try:
        os.unlink(tmp)
      except OSError:
        pass


def read_deps(path):
  if os.path.exists(path):
    with open(path, 'rb') as f:
      return ast.literal_eval(f.read())


def merge_deps(paths):
  deps = {}
  for path in paths:
    d = read_deps(path)
    for key in d:
      if key in deps:
        raise DepsConflictException(key)
    deps.update(d)
  return deps
