# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Validation helpers for simple input validation in the API.

Note: Every validator MUST respect config.do_validation. This is an internally
set config option that allows the mock call decorators to be placed before or
after the validation decorators, rather than forcing an ordering that could then
produce incorrect outputs if missed.
"""

from __future__ import print_function

import functools
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

from google.protobuf import message as protobuf_message


def _value(field, message):
  """Helper function to fetch the value of the field.

  Args:
    field (str): The field name. Can be nested via . separation.
    message (Message): The protobuf message it is being fetched from.

  Returns:
    str|None|int|list|Message|bool - The value of the field.
  """
  value = message
  for part in field.split('.'):
    if not isinstance(value, protobuf_message.Message):
      value = None
      break

    try:
      value = getattr(value, part)
    except AttributeError as e:
      cros_build_lib.Die('Invalid field: %s', e.message)

  return value


#pylint: disable=docstring-misnamed-args
def exists(*fields):
  """Validate that the paths in |fields| exist.

  Args:
    fields (str): The fields being checked. Can be . separated nested
      fields.
  """
  assert fields

  def decorator(func):
    @functools.wraps(func)
    def _exists(input_proto, output_proto, config, *args, **kwargs):
      if config.do_validation:
        for field in fields:
          logging.debug('Validating %s exists.', field)

          value = _value(field, input_proto)
          if not value or not os.path.exists(value):
            cros_build_lib.Die('%s path does not exist: %s' % (field, value))

      return func(input_proto, output_proto, config, *args, **kwargs)

    return _exists

  return decorator


def is_in(field, values):
  """Validate |field| does not contain |value|.

  Args:
    field (str): The field being checked. May be . separated nested fields.
    values (list): The possible values field may take.
  """
  assert field
  assert values

  def decorator(func):
    @functools.wraps(func)
    def _is_in(input_proto, output_proto, config, *args, **kwargs):
      if config.do_validation:
        logging.debug('Validating %s is in %r', field, values)
        value = _value(field, input_proto)

        if value not in values:
          cros_build_lib.Die('%s (%r) must be in %r', field, value, values)

      return func(input_proto, output_proto, config, *args, **kwargs)

    return _is_in

  return decorator


#pylint: disable=docstring-misnamed-args
def require(*fields):
  """Verify |fields| have all been set.

  Args:
    fields (str): The fields being checked. May be . separated nested fields.
  """
  assert fields

  def decorator(func):
    @functools.wraps(func)
    def _require(input_proto, output_proto, config, *args, **kwargs):
      if config.do_validation:
        for field in fields:
          logging.debug('Validating %s is set.', field)

          value = _value(field, input_proto)
          if not value:
            cros_build_lib.Die('%s is required.', field)

      return func(input_proto, output_proto, config, *args, **kwargs)

    return _require

  return decorator


def validation_complete(func):
  """Automatically skip the endpoint when called after all other validators.

  This decorator MUST be applied after all other validate decorators.
  The config can be checked manually if there is non-decorator validation, but
  this is much cleaner if it is all done in decorators.
  """

  @functools.wraps(func)
  def _validate_only(request, response, configs, *args, **kwargs):
    if configs.validate_only:
      # Avoid calling the endpoint.
      return 0
    else:
      return func(request, response, configs, *args, **kwargs)

  return _validate_only
