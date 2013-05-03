# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

from chromite.lib import partial_mock


class ManifestMock(partial_mock.PartialMock):
  """Partial mock for git.Manifest."""
  TARGET = 'chromite.lib.git.Manifest'
  ATTRS = ('_RunParser',)

  def _RunParser(self, *_args):
    pass


class ManifestCheckoutMock(partial_mock.PartialMock):
  """Partial mock for git.ManifestCheckout."""
  TARGET = 'chromite.lib.git.ManifestCheckout'
  ATTRS = ('_GetManifestsBranch',)

  def _GetManifestsBranch(self, _root):
    return 'default'
