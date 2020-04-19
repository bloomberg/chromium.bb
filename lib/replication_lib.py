# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An implementation of the ReplicationConfig proto interface."""

from __future__ import print_function

import json
import os
import shutil
import sys

from chromite.api.gen.config import replication_config_pb2

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.utils import field_mask_util


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def _ValidateFileReplicationRule(rule):
  """Raises an error if a FileReplicationRule is invalid.

  For example, checks that if REPLICATION_TYPE_FILTER, destination_fields
  are specified.

  Args:
    rule: (FileReplicationRule) The rule to validate.
  """
  if rule.file_type == replication_config_pb2.FILE_TYPE_JSON:
    if rule.replication_type != replication_config_pb2.REPLICATION_TYPE_FILTER:
      raise ValueError(
          'Rule for JSON source %s must use REPLICATION_TYPE_FILTER.' %
          rule.source_path)
  elif rule.file_type == replication_config_pb2.FILE_TYPE_OTHER:
    if rule.replication_type != replication_config_pb2.REPLICATION_TYPE_COPY:
      raise ValueError('Rule for source %s must use REPLICATION_TYPE_COPY.' %
                       rule.source_path)
  else:
    raise NotImplementedError('Replicate not implemented for file type %s' %
                              rule.file_type)

  if rule.replication_type == replication_config_pb2.REPLICATION_TYPE_COPY:
    if rule.destination_fields.paths:
      raise ValueError(
          'Rule with REPLICATION_TYPE_COPY cannot use destination_fields.')
  elif rule.replication_type == replication_config_pb2.REPLICATION_TYPE_FILTER:
    if not rule.destination_fields.paths:
      raise ValueError(
          'Rule with REPLICATION_TYPE_FILTER must use destination_fields.')
  else:
    raise NotImplementedError(
        'Replicate not implemented for replication type %s' %
        rule.replication_type)

  if os.path.isabs(rule.source_path) or os.path.isabs(rule.destination_path):
    raise ValueError(
        'Only paths relative to the source root are allowed. In rule: %s' %
        rule)


def _ApplyStringReplacementRules(destination_path, rules):
  """Read the file at destination path, apply rules, and write a new file.

  Args:
    destination_path: (str) Path to the destination file to read. The new file
      will also be written at this path.
    rules: (list[StringReplacementRule]) Rules to apply. Must not be empty.
  """
  assert rules

  with open(destination_path, 'r') as f:
    dst_data = f.read()

  for string_replacement_rule in rules:
    dst_data = dst_data.replace(string_replacement_rule.before,
                                string_replacement_rule.after)

  with open(destination_path, 'w') as f:
    f.write(dst_data)


def Replicate(replication_config):
  """Run the replication described in replication_config.

  Args:
    replication_config: (ReplicationConfig) Describes the replication to run.
  """
  # Validate all rules before any of them are run, to decrease chance of ending
  # with a partial replication.
  for rule in replication_config.file_replication_rules:
    _ValidateFileReplicationRule(rule)

  for rule in replication_config.file_replication_rules:
    logging.info('Processing FileReplicationRule: %s', rule)

    src = os.path.join(constants.SOURCE_ROOT, rule.source_path)
    dst = os.path.join(constants.SOURCE_ROOT, rule.destination_path)

    osutils.SafeMakedirs(os.path.dirname(dst))

    if rule.file_type == replication_config_pb2.FILE_TYPE_JSON:
      assert (rule.replication_type ==
              replication_config_pb2.REPLICATION_TYPE_FILTER)
      assert rule.destination_fields.paths

      with open(src, 'r') as f:
        source_json = json.load(f)

      try:
        source_device_configs = source_json['chromeos']['configs']
      except KeyError:
        raise NotImplementedError(
            ('Currently only ChromeOS Configs are supported (expected file %s '
             'to have a list at "$.chromeos.configs")') % src)

      destination_device_configs = []
      for source_device_config in source_device_configs:
        destination_device_configs.append(
            field_mask_util.CreateFilteredDict(rule.destination_fields,
                                               source_device_config))

      destination_json = {'chromeos': {'configs': destination_device_configs}}

      logging.info('Writing filtered JSON source to %s', dst)
      with open(dst, 'w') as f:
        # Use the print function, so the file ends in a newline.
        print(
            json.dumps(
                destination_json,
                sort_keys=True,
                indent=2,
                separators=(',', ': ')),
            file=f)
    else:
      assert rule.file_type == replication_config_pb2.FILE_TYPE_OTHER
      assert (
          rule.replication_type == replication_config_pb2.REPLICATION_TYPE_COPY)
      assert not rule.destination_fields.paths

      logging.info('Copying full file from %s to %s', src, dst)
      shutil.copy2(src, dst)

    if rule.string_replacement_rules:
      _ApplyStringReplacementRules(dst, rule.string_replacement_rules)
