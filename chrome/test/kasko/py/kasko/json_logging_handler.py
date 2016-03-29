#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging handler used to log into a JSON file."""

import json
import logging


class JSONLoggingHandler(logging.Handler):
  """A logging handler that forwards log messages into a JSON file."""

  def __init__(self, json_file):
    logging.Handler.__init__(self)
    formatter = logging.Formatter('%(name)s:%(message)s')
    self.setFormatter(formatter)
    self.json_file_ = json_file
    self.log_messages_ = []

  def close(self):
    """Dump the list of log messages into the JSON file."""
    with open(self.json_file_, 'w') as f:
      f.write(json.dumps(self.log_messages_))
    logging.Handler.close(self)

  def emit(self, record):
    """Append the record to list of messages."""
    self.log_messages_.append({record.levelname: self.format(record)})
