#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import base64

from writers import adml_writer


def GetWriter(config):
  '''Factory method for creating ADMLWriter objects for the Chrome OS platform.
  See the constructor of TemplateWriter for description of arguments.
  '''
  return adml_writer.ADMLWriter(['chrome_os'], config)
