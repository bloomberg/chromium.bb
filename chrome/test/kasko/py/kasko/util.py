#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Various utilities useful for performing Kasko integration tests."""

import logging
import os
import shutil
import tempfile


_LOGGER = logging.getLogger(os.path.basename(__file__))


class ScopedTempDir(object):
  """A class that creates a scoped temporary directory."""

  def __init__(self):
    self.path_ = None

  def __enter__(self):
    """Creates the temporary directory and initializes |path|."""
    self.path_ = tempfile.mkdtemp(prefix='kasko_integration_')
    return self

  def __exit__(self, *args, **kwargs):
    """Destroys the temporary directory."""
    if self.path_ is None:
      return
    shutil.rmtree(self.path_)

  @property
  def path(self):
    return self.path_

  def release(self):
    path = self.path_
    self.path_ = None
    return path


class ScopedStartStop(object):
  """Utility class for calling 'start' and 'stop' within a scope."""

  def __init__(self, service, start=None, stop=None):
    self.service_ = service

    if start is None:
      self.start_ = lambda x: x.start()
    else:
      self.start_ = start

    if stop is None:
      self.stop_ = lambda x: x.stop()
    else:
      self.stop_ = stop

  def __enter__(self):
    self.start_(self.service_)
    return self

  def __exit__(self, *args, **kwargs):
    if self.service_:
      self.stop_(self.service_)

  @property
  def service(self):
    """Returns the encapsulated service, retaining ownership."""
    return self.service_

  def release(self):
    """Relinquishes ownership of the encapsulated service and returns it."""
    service = self.service_
    self.service_ = None
    return service
