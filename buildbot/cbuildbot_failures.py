# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes of failure types."""

import sys

from chromite.buildbot import cbuildbot_results as results_lib


class SetFailureType(object):
  """A wrapper to re-raise the exception as the pre-set type."""

  def __init__(self, category_exception):
    """Initializes the decorator.

    Args:
      category_exception: The exception type to re-raise as.
    """
    self.category_exception = category_exception

  def __call__(self, functor):
    """Returns a wrapped function."""
    def wrapped_functor(*args):
      try:
        functor(*args)
      except Exception:
        exc_type, exc_value, exc_traceback = sys.exc_info()
        if issubclass(exc_type, self.category_exception):
          # Do not re-raise if the exception is a subclass of the set
          # exception type because it offers more information.
          raise
        else:
          raise self.category_exception, exc_value, exc_traceback

    return wrapped_functor


class InfrastructureFailure(results_lib.StepFailure):
  """Raised if a stage fails due to infrastructure issues."""


# Chrome OS Test Lab failures.
class TestLabFailure(InfrastructureFailure):
  """Raised if a stage fails due to hardware lab infrastructure issues."""


# Gerrit-on-Borg failures.
class GoBFailure(InfrastructureFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) issues."""


class GoBQueryFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) query errors."""


class GoBSubmitFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) submission errors."""


class GoBFetchFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) fetch errors."""


# Google Storage failures.
class GSFailure(InfrastructureFailure):
  """Raised if a stage fails due to Google Storage (GS) issues."""


class GSUploadFailure(GSFailure):
  """Raised if a stage fails due to Google Storage (GS) upload issues."""


class GSDownloadFailure(GSFailure):
  """Raised if a stage fails due to Google Storage (GS) download issues."""


# Builder failures.
class BuilderFailure(InfrastructureFailure):
  """Raised if a stage fails due to builder issues."""


# Crash collection service failures.
class CrashCollectionFailure(InfrastructureFailure):
  """Raised if a stage fails due to crash collection services."""
