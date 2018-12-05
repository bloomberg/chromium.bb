# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The build API entry point."""

from __future__ import print_function

import importlib
import os

from google.protobuf import empty_pb2
from google.protobuf import json_format
from google.protobuf import symbol_database

from chromite.api import service
from chromite.api.gen import build_api_pb2
from chromite.lib import commandline
from chromite.lib import osutils


class Error(Exception):
  """Base error class for the module."""


# API Service Errors.
class UnknownServiceError(Error):
  """Error raised when the requested service has not been registered."""


class ServiceModuleNotDefinedError(Error):
  """Error class for when no module is defined for a service."""


class ServiceModuleNotFoundError(Error):
  """Error raised when the service cannot be imported."""


# API Method Errors.
class UnknownMethodError(Error):
  """The requested service exists but does not have the requested method."""


class MethodNotFoundError(Error):
  """Error raised when the method cannot be found in the service module."""


def GetParser():
  """Build the argument parser.

  The API parser comprises a subparser hierarchy. The general form is:
  `script service method`, e.g. `build_api image test`.
  """
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('service_method',
                      help='The "chromite.api.Service/Method" that is being '
                           'called.')

  parser.add_argument(
      '--input-json', type='path',
      help='Path to the JSON serialized input argument protobuf message.')
  parser.add_argument(
      '--output-json', type='path',
      help='The path to which the result protobuf message should be written.')

  return parser


def _ParseArgs(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  parts = opts.service_method.split('/')

  if len(parts) != 2:
    parser.error('Must pass "Service/Method".')

  opts.service = parts[0]
  opts.method = parts[1]

  opts.Freeze()
  return opts


class Router(object):
  """Encapsulates the request dispatching logic."""

  def __init__(self):
    self._services = {}
    self._aliases = {}
    # All imported generated messages get added to this symbol db.
    self._sym_db = symbol_database.Default()

    extensions = build_api_pb2.DESCRIPTOR.extensions_by_name
    self._service_options = extensions['service_options']
    self._method_options = extensions['method_options']

  def Register(self, proto_module):
    """Register the services from a generated proto module.

    Args:
      proto_module (module): The generated proto module whose service is being
        registered.

    Raises:
      ServiceModuleNotDefinedError when the service cannot be found in the
        provided module.
    """
    services = proto_module.DESCRIPTOR.services_by_name
    for service_name, svc in services.items():
      module_name = svc.GetOptions().Extensions[self._service_options].module

      if not module_name:
        raise ServiceModuleNotDefinedError(
            'The module must be defined in the service definition: %s.%s' %
            (proto_module, service_name))

      self._services[svc.full_name] = (svc, module_name)

  def Route(self, service_name, method_name, input_json):
    """Dispatch the request.

    Args:
      service_name (str): The fully qualified service name.
      method_name (str): The name of the method being called.
      input_json (str): The JSON encoded input message data.

    Returns:
      google.protobuf.message.Message: An instance of the method's output
        message class.

    Raises:
      ServiceModuleNotFoundError when the service module cannot be imported.
      MethodNotFoundError when the method cannot be retrieved from the module.
    """
    try:
      svc, module_name = self._services[service_name]
    except KeyError:
      raise UnknownServiceError('The %s service has not been registered.'
                                % service_name)

    try:
      method_desc = svc.methods_by_name[method_name]
    except KeyError:
      raise UnknownMethodError('The %s method has not been defined in the %s '
                               'service.' % (method_name, service_name))

    # Service method argument magic: do not pass the arguments when the method
    # is expecting the Empty message. Additions of optional arguments/return
    # values are still backwards compatible, but the implementation signature
    # is simplified and more explicit about what its expecting.
    args = []
    # Parse the input file to build an instance of the input message.
    input_msg = self._sym_db.GetPrototype(method_desc.input_type)()
    if not isinstance(input_msg, empty_pb2.Empty):
      json_format.Parse(input_json, input_msg, ignore_unknown_fields=True)
      args.append(input_msg)

    # Get an empty output message instance.
    output_msg = self._sym_db.GetPrototype(method_desc.output_type)()
    if not isinstance(output_msg, empty_pb2.Empty):
      args.append(output_msg)

    # TODO(saklein) Do we need this? Are aliases useful? Maybe dump it.
    # Allow proto-based method name override.
    method_options = method_desc.GetOptions().Extensions[self._method_options]
    if method_options.HasField('implementation_name'):
      method_name = method_options.implementation_name

    # Import the module and get the method.
    method_impl = self._GetMethod(module_name, method_name)

    # Successfully located; call and return.
    method_impl(*args)
    return output_msg

  def _GetMethod(self, module_name, method_name):
    """Get the implementation of the method for the service module.

    Args:
      module_name (str): The name of the service module.
      method_name (str): The name of the method.

    Returns:
      callable - The method.

    Raises:
      MethodNotFoundError when the method cannot be found in the module.
      ServiceModuleNotFoundError when the service module cannot be imported.
    """
    try:
      module = importlib.import_module(service.IMPORT_PATTERN % module_name)
    except ImportError as e:
      raise ServiceModuleNotFoundError(e.message)
    try:
      return getattr(module, method_name)
    except AttributeError as e:
      raise MethodNotFoundError(e.message)


def RegisterServices(router):
  """Register all the services.

  Args:
    router (Router): The router.
  """
  pass


def main(argv):
  opts = _ParseArgs(argv)

  router = Router()
  RegisterServices(router)

  if os.path.exists(opts.input_json):
    input_proto = osutils.ReadFile(opts.input_json)
  else:
    input_proto = None

  output_msg = router.Route(opts.service, opts.method, input_proto)

  if opts.output_json:
    output_content = json_format.MessageToJson(output_msg)
    osutils.WriteFile(opts.output_json, output_content)
