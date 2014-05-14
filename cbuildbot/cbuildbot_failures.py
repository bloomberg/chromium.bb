# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes of failure types."""

import sys

from chromite.lib import cros_build_lib


class SetFailureType(object):
  """A wrapper to re-raise the exception as the pre-set type."""

  def __init__(self, category_exception):
    """Initializes the decorator.

    Args:
      category_exception: The exception type to re-raise as. The
        constructor must accept a string as the argument.
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
          # Convert exc_value to a string to initialize the new
          # exception.
          raise self.category_exception, (str(exc_value),), exc_traceback

    return wrapped_functor


class StepFailure(Exception):
  """StepFailure exceptions indicate that a cbuildbot step failed.

  Exceptions that derive from StepFailure should meet the following
  criteria:
    1) The failure indicates that a cbuildbot step failed.
    2) The necessary information to debug the problem has already been
       printed in the logs for the stage that failed.
    3) __str__() should be brief enough to include in a Commit Queue
       failure message.
  """
  def __init__(self, message='', possibly_flaky=False):
    """Constructor.

    Args:
      message: An error message.
      possibly_flaky: Whether this failure might be flaky.
    """
    Exception.__init__(self, message)
    self.possibly_flaky = possibly_flaky
    self.args = (message, possibly_flaky)

  def __str__(self):
    """Stringify the message."""
    return self.message


class RetriableStepFailure(StepFailure):
  """This exception is thrown when a step failed, but should be retried."""


class BuildScriptFailure(StepFailure):
  """This exception is thrown when a build command failed.

  It is intended to provide a shorter summary of what command failed,
  for usage in failure messages from the Commit Queue, so as to ensure
  that developers aren't spammed with giant error messages when common
  commands (e.g. build_packages) fail.
  """

  def __init__(self, exception, shortname, possibly_flaky=False):
    """Construct a BuildScriptFailure object.

    Args:
      exception: A RunCommandError object.
      shortname: Short name for the command we're running.
      possibly_flaky: Whether this failure might be flaky.
    """
    StepFailure.__init__(self, possibly_flaky=possibly_flaky)
    assert isinstance(exception, cros_build_lib.RunCommandError)
    self.exception = exception
    self.shortname = shortname
    self.args = (exception, shortname, possibly_flaky)

  def __str__(self):
    """Summarize a build command failure briefly."""
    result = self.exception.result
    if result.returncode:
      return '%s failed (code=%s)' % (self.shortname, result.returncode)
    else:
      return self.exception.msg


class PackageBuildFailure(BuildScriptFailure):
  """This exception is thrown when packages fail to build."""

  def __init__(self, exception, shortname, failed_packages):
    """Construct a PackageBuildFailure object.

    Args:
      exception: The underlying exception.
      shortname: Short name for the command we're running.
      failed_packages: List of packages that failed to build.
    """
    BuildScriptFailure.__init__(self, exception, shortname)
    self.failed_packages = set(failed_packages)
    self.args = (exception, shortname, failed_packages)

  def __str__(self):
    return ('Packages failed in %s: %s'
            % (self.shortname, ' '.join(sorted(self.failed_packages))))


class InfrastructureFailure(StepFailure):
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
