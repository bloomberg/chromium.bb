# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.base import output_manager

# TODO(jbudorick): This class is currently mostly unused.
# Add a --bot-mode argument that all bots pass. If --bot-mode and
# --local-output args are both not passed to test runner then use this
# as the output manager impl.

# pylint: disable=abstract-method
# pylint: disable=no-self-use
# pylint: disable=super-init-not-called
# pylint: disable=unused-argument

class NoopOutputManager(output_manager.OutputManager):

  def __init__(self):
    super(NoopOutputManager, self).__init__()


class NoopArchivedFile(output_manager.ArchivedFile):

  def __init__(self):
    pass

  @property
  def name(self):
    return ''

  def write(self, *args, **kwargs):
    pass

  def flush(self, *args, **kwargs):
    pass

  def Link(self):
    return ''

  def Archive(self):
    pass

  def Delete(self):
    pass
