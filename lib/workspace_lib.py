# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common workspace related utilities."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants


def WorkspacePath():
  """Returns the path to the current workspace.

  This path should be correct both inside and outside the chroot.
  TODO(dgarrett): update this once the workspace can be outside of the checkout.
  """
  return os.path.join(constants.SOURCE_ROOT, 'projects')
