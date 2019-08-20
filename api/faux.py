# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Decorators to help handle mock calls and errors in the Build API."""

from __future__ import print_function

import functools

from chromite.api import controller


def all_responses(faux_result_factory):
  """A decorator to handle all mock responses.

  This is syntactic sugar for handling all of the mock response types in a
  single function.

  Args:
    faux_result_factory: A function with the same signature as a regular
        API endpoint function that populates the output with success or error
        results, as requested, without executing the endpoint.
  """
  assert faux_result_factory

  # Get the decorators for each of the mock types so we can compose them.
  success_fn = success(faux_result_factory)
  err_fn = error(faux_result_factory)

  def _decorator(func):
    return err_fn(success_fn(func))

  return _decorator


def all_empty(func):
  """Decorator to handle all mock responses with an empty output."""
  return empty_error(empty_success(func))


def success(faux_result_factory):
  """A decorator to handle mock call responses.

  Args:
    faux_result_factory: A function with the same signature as a regular
        API endpoint function that populates the output
  """
  assert faux_result_factory

  def decorator(func):

    @functools.wraps(func)
    def _success(input_proto, output_proto, config, *args, **kwargs):
      if config.mock_call:
        faux_result_factory(input_proto, output_proto, config, *args, **kwargs)
        return controller.RETURN_CODE_SUCCESS

      return func(input_proto, output_proto, config, *args, **kwargs)

    return _success

  return decorator


def empty_success(func):
  """A decorator to handle mock success responses with empty outputs."""
  @functools.wraps(func)
  def _empty_success(input_proto, output_proto, config, *args, **kwargs):
    if config.mock_call:
      return controller.RETURN_CODE_SUCCESS

    return func(input_proto, output_proto, config, *args, **kwargs)

  return _empty_success


def error(faux_error_factory):
  """A decorator to handle mock error responses."""
  assert faux_error_factory

  def decorator(func):

    @functools.wraps(func)
    def _error(input_proto, output_proto, config, *args, **kwargs):
      if config.mock_error:
        faux_error_factory(input_proto, output_proto, config, *args, **kwargs)
        return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

      return func(input_proto, output_proto, config, *args, **kwargs)

    return _error

  return decorator


def empty_error(func):
  """A decorator to handle mock error responses with empty outputs."""
  @functools.wraps(func)
  def _empty_error(input_proto, output_proto, config, *args, **kwargs):
    if config.mock_error:
      return controller.RETURN_CODE_UNRECOVERABLE

    return func(input_proto, output_proto, config, *args, **kwargs)

  return _empty_error
