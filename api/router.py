# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Router class for the Build API.

Handles routing requests to the appropriate controller and handles service
registration.
"""

from __future__ import print_function

import importlib
import os

from google.protobuf import json_format
from google.protobuf import symbol_database

from chromite.api import controller
from chromite.api import field_handler
from chromite.api.gen.chromite.api import android_pb2
from chromite.api.gen.chromite.api import api_pb2
from chromite.api.gen.chromite.api import artifacts_pb2
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.api.gen.chromite.api import build_api_pb2
from chromite.api.gen.chromite.api import depgraph_pb2
from chromite.api.gen.chromite.api import image_pb2
from chromite.api.gen.chromite.api import packages_pb2
from chromite.api.gen.chromite.api import sdk_pb2
from chromite.api.gen.chromite.api import sysroot_pb2
from chromite.api.gen.chromite.api import test_pb2
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class Error(Exception):
  """Base error class for the module."""


class InvalidInputFileError(Error):
  """Raised when the input file cannot be read."""


class InvalidInputFormatError(Error):
  """Raised when the passed input protobuf can't be parsed."""


class InvalidOutputFileError(Error):
  """Raised when the output file cannot be written."""


class CrosSdkNotRunError(Error):
  """Raised when the cros_sdk command could not be run to enter the chroot."""


# API Service Errors.
class UnknownServiceError(Error):
  """Error raised when the requested service has not been registered."""


class ControllerModuleNotDefinedError(Error):
  """Error class for when no controller is defined for a service."""


class ServiceControllerNotFoundError(Error):
  """Error raised when the service's controller cannot be imported."""


# API Method Errors.
class UnknownMethodError(Error):
  """The service is defined in the proto but the method is not."""


class MethodNotFoundError(Error):
  """The method's implementation cannot be found in the service's controller."""


class Router(object):
  """Encapsulates the request dispatching logic."""

  REEXEC_INPUT_FILE = 'input.json'
  REEXEC_OUTPUT_FILE = 'output.json'

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
        raise ControllerModuleNotDefinedError(
            'The module must be defined in the service definition: %s.%s' %
            (proto_module, service_name))

      self._services[svc.full_name] = (svc, module_name)

  def ListMethods(self):
    """List all methods registered with the router."""
    services = []
    for service_name, (svc, _module) in self._services.items():
      for method_name in svc.methods_by_name.keys():
        services.append('%s/%s' % (service_name, method_name))

    return sorted(services)

  def Route(self, service_name, method_name, input_path, output_path, config):
    """Dispatch the request.

    Args:
      service_name (str): The fully qualified service name.
      method_name (str): The name of the method being called.
      input_path (str): The path to the input message file.
      output_path (str): The path where the output message should be written.
      config (api_config.ApiConfig): The optional call configs.

    Returns:
      int: The return code.

    Raises:
      InvalidInputFileError when the input file cannot be read.
      InvalidOutputFileError when the output file cannot be written.
      ServiceModuleNotFoundError when the service module cannot be imported.
      MethodNotFoundError when the method cannot be retrieved from the module.
    """
    try:
      input_json = osutils.ReadFile(input_path).strip()
    except IOError as e:
      raise InvalidInputFileError('Unable to read input file: %s' % e)

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

    # Parse the input file to build an instance of the input message.
    input_msg = self._sym_db.GetPrototype(method_desc.input_type)()
    try:
      json_format.Parse(input_json, input_msg, ignore_unknown_fields=True)
    except json_format.ParseError as e:
      raise InvalidInputFormatError('Unable to parse the input json: %s' % e)

    # Get an empty output message instance.
    output_msg = self._sym_db.GetPrototype(method_desc.output_type)()

    # Fetch the method options for chroot and method name overrides.
    method_options = method_desc.GetOptions().Extensions[self._method_options]

    # Check the chroot settings before running.
    service_options = svc.GetOptions().Extensions[self._service_options]
    if self._ChrootCheck(service_options, method_options):
      # Run inside the chroot instead.
      logging.info('Re-executing the endpoint inside the chroot.')
      return self._ReexecuteInside(input_msg, output_msg, output_path,
                                   service_name, method_name, config)

    # Allow proto-based method name override.
    if method_options.HasField('implementation_name'):
      method_name = method_options.implementation_name

    # Import the module and get the method.
    method_impl = self._GetMethod(module_name, method_name)

    # Successfully located; call and return.
    return_code = method_impl(input_msg, output_msg, config)
    if return_code is None:
      return_code = controller.RETURN_CODE_SUCCESS

    try:
      osutils.WriteFile(output_path, json_format.MessageToJson(output_msg))
    except IOError as e:
      raise InvalidOutputFileError('Cannot write output file: %s' % e)

    return return_code

  def _ChrootCheck(self, service_options, method_options):
    """Check the chroot options, and execute assertion or note reexec as needed.

    Args:
      service_options (google.protobuf.Message): The service options.
      method_options (google.protobuf.Message): The method options.

    Returns:
      bool - True iff it needs to be reexeced inside the chroot.

    Raises:
      cros_build_lib.DieSystemExit when the chroot setting cannot be satisfied.
    """
    chroot_assert = build_api_pb2.NO_ASSERTION
    if method_options.HasField('method_chroot_assert'):
      # Prefer the method option when set.
      chroot_assert = method_options.method_chroot_assert
    elif service_options.HasField('service_chroot_assert'):
      # Fall back to the service option.
      chroot_assert = service_options.service_chroot_assert

    if chroot_assert == build_api_pb2.INSIDE:
      return not cros_build_lib.IsInsideChroot()
    elif chroot_assert == build_api_pb2.OUTSIDE:
      # If it must be run outside we have to already be outside.
      cros_build_lib.AssertOutsideChroot()

    return False

  def _ReexecuteInside(self, input_msg, output_msg, output_path, service_name,
                       method_name, config):
    """Re-execute the service inside the chroot.

    Args:
      input_msg (Message): The parsed input message.
      output_msg (Message): The empty output message instance.
      output_path (str): The path for the serialized output.
      service_name (str): The name of the service to run.
      method_name (str): The name of the method to run.
      config (api_config.ApiConfig): The optional call configs.
    """
    # Parse the chroot and clear the chroot field in the input message.
    chroot = field_handler.handle_chroot(input_msg)

    with field_handler.copy_paths_in(input_msg, chroot.tmp, prefix=chroot.path):
      with chroot.tempdir() as tempdir:
        new_input = os.path.join(tempdir, self.REEXEC_INPUT_FILE)
        chroot_input = '/%s' % os.path.relpath(new_input, chroot.path)
        new_output = os.path.join(tempdir, self.REEXEC_OUTPUT_FILE)
        chroot_output = '/%s' % os.path.relpath(new_output, chroot.path)

        logging.info('Writing input message to: %s', new_input)
        osutils.WriteFile(new_input, json_format.MessageToJson(input_msg))
        osutils.Touch(new_output)

        cmd = ['build_api', '%s/%s' % (service_name, method_name),
               '--input-json', chroot_input, '--output-json', chroot_output]

        if config.validate_only:
          cmd.append('--validate-only')

        try:
          result = cros_build_lib.run(
              cmd,
              enter_chroot=True,
              chroot_args=chroot.get_enter_args(),
              error_code_ok=True,
              extra_env=chroot.env)
        except cros_build_lib.RunCommandError:
          # A non-zero return code will not result in an error, but one is still
          # thrown when the command cannot be run in the first place. This is
          # known to happen at least when the PATH does not include the chromite
          # bin dir.
          raise CrosSdkNotRunError('Unable to enter the chroot.')

        logging.info('Endpoint execution completed, return code: %d',
                     result.returncode)

        # Transfer result files out of the chroot.
        output_content = osutils.ReadFile(new_output)
        if output_content:
          json_format.Parse(output_content, output_msg)
          field_handler.extract_results(input_msg, output_msg, chroot)

        osutils.WriteFile(output_path, json_format.MessageToJson(output_msg))

        return result.returncode

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
      module = importlib.import_module(controller.IMPORT_PATTERN % module_name)
    except ImportError as e:
      raise ServiceControllerNotFoundError(str(e))
    try:
      return getattr(module, method_name)
    except AttributeError as e:
      raise MethodNotFoundError(str(e))


def RegisterServices(router):
  """Register all the services.

  Args:
    router (Router): The router.
  """
  router.Register(android_pb2)
  router.Register(api_pb2)
  router.Register(artifacts_pb2)
  router.Register(binhost_pb2)
  router.Register(depgraph_pb2)
  router.Register(image_pb2)
  router.Register(packages_pb2)
  router.Register(sdk_pb2)
  router.Register(sysroot_pb2)
  router.Register(test_pb2)
  router.Register(toolchain_pb2)
  logging.debug('Services registered successfully.')


def GetRouter():
  """Get a router that has had all of the services registered."""
  router = Router()
  RegisterServices(router)

  return router
