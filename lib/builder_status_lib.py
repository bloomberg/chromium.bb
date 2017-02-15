# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage builder statuses."""

from __future__ import print_function

import cPickle

from chromite.lib import constants


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
