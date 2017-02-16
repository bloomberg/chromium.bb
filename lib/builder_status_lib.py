# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage builder statuses."""

from __future__ import print_function

import cPickle
import os

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import gs


site_config = config_lib.GetConfig()

BUILD_STATUS_URL = (
    '%s/builder-status' % site_config.params.MANIFEST_VERSIONS_GS_URL)
NUM_RETRIES = 20


class BuilderStatus(object):
  """Object representing the status of a build."""

  def __init__(self, status, message, dashboard_url=None):
    """Constructor for BuilderStatus.

    Args:
      status: Status string (should be one of BUILDER_STATUS_FAILED,
              BUILDER_STATUS_PASSED, BUILDER_STATUS_INFLIGHT, or
              BUILDER_STATUS_MISSING).
      message: A failures_lib.BuildFailureMessage object with details
               of builder failure. Or, None.
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    self.status = status
    self.message = message
    self.dashboard_url = dashboard_url

  # Helper methods to make checking the status object easy.

  def Failed(self):
    """Returns True if the Builder failed."""
    return self.status == constants.BUILDER_STATUS_FAILED

  def Passed(self):
    """Returns True if the Builder passed."""
    return self.status == constants.BUILDER_STATUS_PASSED

  def Inflight(self):
    """Returns True if the Builder is still inflight."""
    return self.status == constants.BUILDER_STATUS_INFLIGHT

  def Missing(self):
    """Returns True if the Builder is missing any status."""
    return self.status == constants.BUILDER_STATUS_MISSING

  def Completed(self):
    """Returns True if the Builder has completed."""
    return self.status in constants.BUILDER_COMPLETED_STATUSES

  @classmethod
  def GetCompletedStatus(cls, success):
    """Return the appropriate status constant for a completed build.

    Args:
      success: Whether the build was successful or not.
    """
    if success:
      return constants.BUILDER_STATUS_PASSED
    else:
      return constants.BUILDER_STATUS_FAILED

  def AsFlatDict(self):
    """Returns a flat json-able representation of this builder status.

    Returns:
      A dictionary of the form {'status' : status, 'message' : message,
      'dashboard_url' : dashboard_url} where all values are guaranteed
      to be strings. If dashboard_url is None, the key will be excluded.
    """
    flat_dict = {'status' : str(self.status),
                 'message' : str(self.message),
                 'reason' : str(None if self.message is None
                                else self.message.reason)}
    if self.dashboard_url is not None:
      flat_dict['dashboard_url'] = str(self.dashboard_url)
    return flat_dict

  def AsPickledDict(self):
    """Returns a pickled dictionary representation of this builder status."""
    return cPickle.dumps(dict(status=self.status, message=self.message,
                              dashboard_url=self.dashboard_url))


class BuilderStatusManager(object):
  """Operations to manage BuilderStatus."""

  @staticmethod
  def GetStatusUrl(builder, version):
    """Get the status URL in Google Storage for a given builder / version."""
    return os.path.join(BUILD_STATUS_URL, version, builder)

  @staticmethod
  def _UnpickleBuildStatus(pickle_string):
    """Returns a builder_status_lib.BuilderStatus obj from a pickled string."""
    try:
      status_dict = cPickle.loads(pickle_string)
    except (cPickle.UnpicklingError, AttributeError, EOFError,
            ImportError, IndexError, TypeError) as e:
      # The above exceptions are listed as possible unpickling exceptions
      # by http://docs.python.org/2/library/pickle
      # In addition to the exceptions listed in the doc, we've also observed
      # TypeError in the wild.
      logging.warning('Failed with %r to unpickle status file.', e)
      return BuilderStatus(
          constants.BUILDER_STATUS_FAILED, message=None)

    return BuilderStatus(**status_dict)

  @staticmethod
  def GetBuilderStatus(builder, version, retries=NUM_RETRIES):
    """Returns a builder_status_lib.BuilderStatus obj for the given the builder.

    Args:
      builder: Builder to look at.
      version: Version string.
      retries: Number of retries for getting the status.

    Returns:
      A builder_status_lib.BuilderStatus instance containing the builder status
      and any optional message associated with the status passed by the builder.
      If no status is found for this builder then the returned
      builder_status_lib.BuilderStatus object will have status STATUS_MISSING.
    """
    url = BuilderStatusManager.GetStatusUrl(builder, version)
    ctx = gs.GSContext(retries=retries)
    try:
      output = ctx.Cat(url)
    except gs.GSNoSuchKey:
      return BuilderStatus(
          constants.BUILDER_STATUS_MISSING, None)

    return BuilderStatusManager._UnpickleBuildStatus(output)
