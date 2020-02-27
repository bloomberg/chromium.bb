# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Router class for the Build API.

Handles routing requests to the appropriate controller and handles service
registration.
"""

from __future__ import print_function

import collections
import importlib
import os
import sys

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
from chromite.api.gen.chromite.api import payload_pb2
from chromite.api.gen.chromite.api import sdk_pb2
from chromite.api.gen.chromite.api import sysroot_pb2
from chromite.api.gen.chromite.api import test_pb2
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.utils import memoize

MethodData = collections.namedtuple(
    'MethodData', ('service_descriptor', 'module_name', 'method_descriptor'))

assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class Error(Exception):
  """Base error class for the module."""


class CrosSdkNotRunError(Error):
  """Raised when the cros_sdk command could not be run to enter the chroot."""


# API Service Errors.
class UnknownServiceError(Error):
  """Error raised when the requested service has not been registered."""


class ControllerModuleNotDefinedError(Error):
  """Error class for when no controller has been defined for a service."""


class ServiceControllerNotFoundError(Error):
  """Error raised when the service's controller cannot be imported."""


# API Method Errors.
class UnknownMethodError(Error):
  """The service has been defined in the proto, but the method has not."""


class MethodNotFoundError(Error):
  """The method's implementation cannot be found in the service's controller."""


class Router(object):
  """Encapsulates the request dispatching logic."""

  REEXEC_INPUT_FILE = 'input_proto'
  REEXEC_OUTPUT_FILE = 'output_proto'
  REEXEC_CONFIG_FILE = 'config_proto'

  def __init__(self):
    self._services = {}
    self._aliases = {}
    # All imported generated messages get added to this symbol db.
    self._sym_db = symbol_database.Default()

    # Save the service and method extension info for looking up
    # configured extension data.
    extensions = build_api_pb2.DESCRIPTOR.extensions_by_name
    self._svc_options_ext = extensions['service_options']
    self._method_options_ext = extensions['method_options']

  @memoize.Memoize
  def _get_method_data(self, service_name, method_name):
    """Get the descriptors and module name for the given Service/Method."""
    try:
      svc, module_name = self._services[service_name]
    except KeyError:
      raise UnknownServiceError(
          'The %s service has not been registered.' % service_name)

    try:
      method_desc = svc.methods_by_name[method_name]
    except KeyError:
      raise UnknownMethodError('The %s method has not been defined in the %s '
                               'service.' % (method_name, service_name))

    return MethodData(
        service_descriptor=svc,
        module_name=module_name,
        method_descriptor=method_desc)

  def _get_input_message_instance(self, service_name, method_name):
    """Get an empty input message instance for the specified method."""
    method_data = self._get_method_data(service_name, method_name)
    return self._sym_db.GetPrototype(method_data.method_descriptor.input_type)()

  def _get_output_message_instance(self, service_name, method_name):
    """Get an empty output message instance for the specified method."""
    method_data = self._get_method_data(service_name, method_name)
    return self._sym_db.GetPrototype(
        method_data.method_descriptor.output_type)()

  def _get_module_name(self, service_name, method_name):
    """Get the name of the module containing the endpoint implementation."""
    return self._get_method_data(service_name, method_name).module_name

  def _get_service_options(self, service_name, method_name):
    """Get the configured service options for the endpoint."""
    method_data = self._get_method_data(service_name, method_name)
    svc_extensions = method_data.service_descriptor.GetOptions().Extensions
    return svc_extensions[self._svc_options_ext]

  def _get_method_options(self, service_name, method_name):
    """Get the configured method options for the endpoint."""
    method_data = self._get_method_data(service_name, method_name)
    method_extensions = method_data.method_descriptor.GetOptions().Extensions
    return method_extensions[self._method_options_ext]

  def Register(self, proto_module):
    """Register the services from a generated proto module.

    Args:
      proto_module (module): The generated proto module to register.

    Raises:
      ServiceModuleNotDefinedError when the service cannot be found in the
        provided module.
    """
    services = proto_module.DESCRIPTOR.services_by_name
    for service_name, svc in services.items():
      module_name = svc.GetOptions().Extensions[self._svc_options_ext].module

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

  def Route(self, service_name, method_name, config, input_handler,
            output_handlers, config_handler):
    """Dispatch the request.

    Args:
      service_name (str): The fully qualified service name.
      method_name (str): The name of the method being called.
      config (api_config.ApiConfig): The call configs.
      input_handler (message_util.MessageHandler): The request message handler.
      output_handlers (list[message_util.MessageHandler]): The response message
        handlers.
      config_handler (message_util.MessageHandler): The config message handler.

    Returns:
      int: The return code.

    Raises:
      InvalidInputFileError when the input file cannot be read.
      InvalidOutputFileError when the output file cannot be written.
      ServiceModuleNotFoundError when the service module cannot be imported.
      MethodNotFoundError when the method cannot be retrieved from the module.
    """
    input_msg = self._get_input_message_instance(service_name, method_name)
    input_handler.read_into(input_msg)

    # Get an empty output message instance.
    output_msg = self._get_output_message_instance(service_name, method_name)

    # Fetch the method options for chroot and method name overrides.
    method_options = self._get_method_options(service_name, method_name)

    # Check the chroot settings before running.
    service_options = self._get_service_options(service_name, method_name)
    if self._ChrootCheck(service_options, method_options):
      # Run inside the chroot instead.
      logging.info('Re-executing the endpoint inside the chroot.')
      return self._ReexecuteInside(input_msg, output_msg, config, input_handler,
                                   output_handlers, config_handler,
                                   service_name, method_name)

    # Allow proto-based method name override.
    if method_options.HasField('implementation_name'):
      implementation_name = method_options.implementation_name
    else:
      implementation_name = method_name

    # Import the module and get the method.
    module_name = self._get_module_name(service_name, method_name)
    method_impl = self._GetMethod(module_name, implementation_name)

    # Successfully located; call and return.
    return_code = method_impl(input_msg, output_msg, config)
    if return_code is None:
      return_code = controller.RETURN_CODE_SUCCESS

    for h in output_handlers:
      h.write_from(output_msg)

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

  def _ReexecuteInside(self, input_msg, output_msg, config, input_handler,
                       output_handlers, config_handler, service_name,
                       method_name):
    """Re-execute the service inside the chroot.

    Args:
      input_msg (Message): The parsed input message.
      output_msg (Message): The empty output message instance.
      config (api_config.ApiConfig): The call configs.
      input_handler (MessageHandler): Input message handler.
      output_handlers (list[MessageHandler]): Output message handlers.
      config_handler (MessageHandler): Config message handler.
      service_name (str): The name of the service to run.
      method_name (str): The name of the method to run.
    """
    # Parse the chroot and clear the chroot field in the input message.
    chroot = field_handler.handle_chroot(input_msg)

    # Use a ContextManagerStack to avoid the deep nesting this many
    # context managers introduces.
    with cros_build_lib.ContextManagerStack() as stack:
      # TempDirs setup.
      tempdir = stack.Add(chroot.tempdir).tempdir
      sync_tempdir = stack.Add(chroot.tempdir).tempdir
      # The copy-paths-in context manager to handle Path messages.
      stack.Add(
          field_handler.copy_paths_in,
          input_msg,
          chroot.tmp,
          prefix=chroot.path)
      # The sync-directories context manager to handle SyncedDir messages.
      stack.Add(
          field_handler.sync_dirs, input_msg, sync_tempdir, prefix=chroot.path)

      chroot.goma = field_handler.handle_goma(input_msg, chroot.path)

      new_input = os.path.join(tempdir, self.REEXEC_INPUT_FILE)
      chroot_input = '/%s' % os.path.relpath(new_input, chroot.path)
      new_output = os.path.join(tempdir, self.REEXEC_OUTPUT_FILE)
      chroot_output = '/%s' % os.path.relpath(new_output, chroot.path)
      new_config = os.path.join(tempdir, self.REEXEC_CONFIG_FILE)
      chroot_config = '/%s' % os.path.relpath(new_config, chroot.path)

      logging.info('Writing input message to: %s', new_input)
      input_handler.write_from(input_msg, path=new_input)
      osutils.Touch(new_output)
      logging.info('Writing config message to: %s', new_config)
      config_handler.write_from(config.get_proto(), path=new_config)

      # We can use a single output to write the rest of them. Use the
      # first one as the reexec output and just translate its output in
      # the rest of the handlers after.
      output_handler = output_handlers[0]

      cmd = [
          'build_api',
          '%s/%s' % (service_name, method_name),
          input_handler.input_arg,
          chroot_input,
          output_handler.output_arg,
          chroot_output,
          config_handler.config_arg,
          chroot_config,
      ]

      try:
        result = cros_build_lib.run(
            cmd,
            enter_chroot=True,
            chroot_args=chroot.get_enter_args(),
            check=False,
            extra_env=chroot.env)
      except cros_build_lib.RunCommandError:
        # A non-zero return code will not result in an error, but one
        # is still thrown when the command cannot be run in the first
        # place. This is known to happen at least when the PATH does
        # not include the chromite bin dir.
        raise CrosSdkNotRunError('Unable to enter the chroot.')

      logging.info('Endpoint execution completed, return code: %d',
                   result.returncode)

      # Transfer result files out of the chroot.
      output_handler.read_into(output_msg, path=new_output)
      field_handler.extract_results(input_msg, output_msg, chroot)

      # Write out all of the response formats.
      for handler in output_handlers:
        handler.write_from(output_msg)

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
  router.Register(payload_pb2)
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
