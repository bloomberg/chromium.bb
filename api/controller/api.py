# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API information controller."""

from __future__ import print_function

from chromite.api import router as router_lib


def GetMethods(_input_proto, output_proto):
  """List all of the registered methods."""
  router = router_lib.GetRouter()
  for method in router.ListMethods():
    output_proto.methods.add().method = method
