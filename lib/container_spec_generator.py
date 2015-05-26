# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic to generate a ContainerSpec from an appc pod manifest.

https://github.com/appc/spec/blob/master/SPEC.md
"""

from __future__ import print_function

import collections
import json
import os
import re

from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import user_db

KEY_ANNOTATIONS_LIST = 'annotations'
KEY_ANNOTATION_NAME = 'name'
KEY_ANNOTATION_VALUE = 'value'
SERVICE_NAME_ANNOTATION_PREFIX = 'bruteus-service-'

KEY_APPS_LIST = 'apps'
KEY_APP_NAME = 'name'
KEY_APP_IMAGE = 'image'
KEY_APP_IMAGE_NAME = 'name'

KEY_APP_SUB_APP = 'app'
KEY_SUB_APP_USER = 'user'
KEY_SUB_APP_GROUP = 'group'
KEY_SUB_APP_EXEC = 'exec'
KEY_SUB_APP_PORTS = 'ports'

PORT_SPEC_NAME = 'name'
PORT_SPEC_PORT = 'port'
PORT_SPEC_PROTOCOL = 'protocol'

PROTOCOL_TCP = 'tcp'
PROTOCOL_UDP = 'udp'
VALID_PROTOCOLS = (PROTOCOL_TCP, PROTOCOL_UDP)

PortSpec = collections.namedtuple('PortSpec', ('allow_all', 'port_list'))


def IsValidAcName(name):
  """Returns true if |name| adheres to appc's AC Name Type.

  This roughly means that a string looks like a protocol-less
  URL (e.g. foo-foo/bar/bar).

  https://github.com/appc/spec/blob/master/SPEC.md#ac-name-type

  Args:
    name: string to validate.

  Returns:
    True iff |name| is a valid AC Name.
  """
  return bool(re.match(r'^[a-z0-9]+([-./][a-z0-9]+)*$', name))


class ContainerSpecWrapper(object):
  """Wrapper that knows how to set fields in a protocol buffer.

  This makes mocking out our protocol buffer interface much simpler.
  """

  def __init__(self):
    # In the context of unittests run from outside the chroot, this import
    # will fail.  Tests will mock out this entire class.
    from generated import soma_container_spec_pb2
    self.container_spec = soma_container_spec_pb2.ContainerSpec()

  def SetName(self, name):
    """Set the name of the runnable brick."""
    self.container_spec.name = name
    self.container_spec.service_bundle_path = '/bricks/%s' % name

  def AddExecutable(self, uid, gid, command_line, tcp_ports, udp_ports):
    """Add an executable to the wrapped ContainerSpec.

    Args:
      uid: integer UID of the user to run this executable.
      gid: integer GID of the group to run this executable.
      command_line: list of strings to run.
      tcp_ports: list of PortSpec tuples.
      udp_ports: list of PortSpec tuples.
    """
    executable = self.container_spec.executables.add()
    executable.uid = uid
    executable.gid = gid
    executable.command_line.extend(command_line)
    for listen_ports, ports in ((executable.tcp_listen_ports, tcp_ports),
                                (executable.udp_listen_ports, udp_ports)):
      if ports.allow_all:
        listen_ports.allow_all = True
      else:
        listen_ports.allow_all = False
        listen_ports.ports.extend(ports.port_list)

  def AddServiceName(self, service_name):
    """Adds the name of an service that'll run inside this container."""
    self.container_spec.service_names.append(service_name)


def _CheckType(instance, expected_type, description):
  """Raise an error if |instance| is not of |expected_type|.

  Args:
    instance: instance of a Python object.
    expected_type: expected type of |instance|.
    description: short string describing |instance| used in error reporting.
  """
  if not isinstance(instance, expected_type):
    raise ValueError(
        'Expected %s to be a %s, but found %s' %
        (description, expected_type.__name__, instance.__class__.__name__))


def _GetValueOfType(a_dict, key, value_type, value_description):
  """Raise an exception if we cannot get |key| from |a_dict| with |value_type|.

  Args:
    a_dict: a dictionary.
    key: string key that should be in the dictionary.
    value_type: expected type of the value at a_dict[key].
    value_description: string describing the value used in error reporting.
  """
  try:
    value = a_dict[key]
  except KeyError:
    raise ValueError('Missing %s in appc pod manifest (key "%s")' %
                     (value_description, key))
  _CheckType(value, value_type, value_description)
  return value


def _GetPortList(desired_protocol, appc_port_list):
  """Get the list of ports opened for |desired_protocol| from |appc_port_list|.

  Args:
    desired_protocol: one of VALID_PROTOCOLS.
    appc_port_list: list of port specifications from a appc pod manifest.

  Returns:
    Instance of PortSpec.
  """
  # The port specification is optional.
  if appc_port_list is None:
    return PortSpec(False, [])

  _CheckType(appc_port_list, list, 'port specification list')

  allow_all = False
  port_list = []
  for port_dict in appc_port_list:
    _CheckType(port_dict, dict, 'port specification')

    # We don't actually use the port name, but it's handy for documentation
    # and standard adherence to enforce its existence.
    port_name = _GetValueOfType(
        port_dict, PORT_SPEC_NAME, unicode, 'port name')
    logging.debug('Validating appc specifcation of "%s"', port_name)
    port = _GetValueOfType(port_dict, PORT_SPEC_PORT, int, 'port')
    protocol = _GetValueOfType(
        port_dict, PORT_SPEC_PROTOCOL, unicode, 'protocol')

    # Validate everything before acting on it.
    if protocol not in VALID_PROTOCOLS:
      raise ValueError('Port protocol must be in %r, not "%s"' %
                       (VALID_PROTOCOLS, protocol))
    if protocol != desired_protocol:
      continue

    if port == -1:
      # Remember that we're going to return that all ports are opened, but
      # continue validating all the remaining specifications.
      allow_all = True
      continue

    # Now we know it's not the wildcard port, and that we've never declared
    # a wildcard for this protocol.
    if port >= 65536 or port <= 0:
      raise ValueError(
          'Port numbers must fit in 16 bits (invalid port=%d).' % port)

    port_list.append(port)

  return PortSpec(allow_all, port_list)


class ContainerSpecGenerator(object):
  """Delegate that knows how to read appc manifests and write ContainerSpecs."""

  def __init__(self, sysroot):
    self._sysroot = sysroot
    self._user_db = user_db.UserDB(sysroot)

  def _CheckAbsPathToExecutable(self, path_to_binary):
    """Raises if there is no exectable at |path_to_binary|."""
    if not os.path.isabs(path_to_binary):
      raise ValueError(
          'Brick executables must be specified by absolute path, not "%s".' %
          path_to_binary)
    return True

  def _FillInServiceNamesFromAnnotations(self, wrapper, annotations):
    """Fill in the ContainerSpec service_names field from |annotations|.

    An appc pod specification can contain a list of (mostly) arbitrary
    annotations that projects can use to add their own metadata fields.
    |annotations| is a list of dicts that each contain a name and value field,
    and this method looks for 'name' fields that are prefixed with
    SERVICE_NAME_ANNOTATION_PREFIX and treats the associated 'value' as the
    name of an service that psyched will expect to be registered from within
    this sandbox.

    Args:
      wrapper: instance of ContainerSpecWrapper.
      annotations: list of dicts, each with a name and value field.
    """
    for annotation in annotations:
      _CheckType(annotation, dict, 'a single annotation')
      name = _GetValueOfType(annotation, KEY_ANNOTATION_NAME, unicode,
                             'annotation name')
      if not IsValidAcName(name):
        raise ValueError('Annotation name "%s" contains illegal characters.' %
                         name)
      if name.startswith(SERVICE_NAME_ANNOTATION_PREFIX):
        service_name = _GetValueOfType(annotation, KEY_ANNOTATION_VALUE,
                                       unicode, 'service name value')
        if not IsValidAcName(name):
          raise ValueError('Service name "%s" contains illegal characters.' %
                           service_name)
        wrapper.AddServiceName(service_name)

  def _FillInExecutableFromApp(self, wrapper, app):
    """Fill in the fields of a ContainerSpec.Executable object from |app|.

    Args:
      wrapper: instance of ContainerSpecWrapper.
      app: dictionary of information taken from the appc pod manifest.
    """
    sub_app = _GetValueOfType(app, KEY_APP_SUB_APP, dict, 'per app app dict')
    user = _GetValueOfType(sub_app, KEY_SUB_APP_USER, unicode, 'app dict user')
    group = _GetValueOfType(
        sub_app, KEY_SUB_APP_GROUP, unicode, 'app dict group')

    if not self._user_db.UserExists(user):
      raise ValueError('Found invalid username "%s"' % user)
    if not self._user_db.GroupExists(group):
      raise ValueError('Found invalid groupname "%s"' % group)

    cmd = _GetValueOfType(sub_app, KEY_SUB_APP_EXEC, list, 'app command line')
    if not cmd:
      raise ValueError('App command line must give the executable to run.')
    self._CheckAbsPathToExecutable(cmd[0])
    for cmd_piece in cmd:
      _CheckType(cmd_piece, unicode, 'app.exec fragment')

    port_list = sub_app.get(KEY_SUB_APP_PORTS, None)
    wrapper.AddExecutable(self._user_db.ResolveUsername(user),
                          self._user_db.ResolveGroupname(group),
                          cmd,
                          _GetPortList(PROTOCOL_TCP, port_list),
                          _GetPortList(PROTOCOL_UDP, port_list))

  def GetContainerSpec(self, appc_contents, container_spec_name):
    """Create a ContainerSpec encoding the information in an appc pod manifest.

    Args:
      appc_contents: string contents of an appc pod manifest
      container_spec_name: string unique name of this container.

    Returns:
      an instance of ContainerSpec.
    """
    wrapper = ContainerSpecWrapper()
    service_bundle_name = None

    for app in _GetValueOfType(appc_contents, KEY_APPS_LIST, list, 'app list'):
      _CheckType(app, dict, 'app')

      # Aid debugging of problems in specific apps.
      app_name = _GetValueOfType(app, KEY_APP_NAME, unicode, 'app name')
      if not IsValidAcName(app_name):
        raise ValueError('Application name "%s" contains illegal characters.' %
                         app_name)
      logging.debug('Processing application "%s".', app_name)

      # Get the name of the image, check that it's consistent other image names.
      image = _GetValueOfType(
          app, KEY_APP_IMAGE, dict, 'image specification for app')
      image_name = _GetValueOfType(
          image, KEY_APP_IMAGE_NAME, unicode, 'image name')
      if not IsValidAcName(image_name):
        raise ValueError('Image name "%s" contains illegal characters.' %
                         image_name)

      if service_bundle_name and service_bundle_name != image_name:
        raise ValueError(
            'All elements of "apps" must have the same image.name.')
      service_bundle_name = image_name

      # Add the executable corresponding to this app to our ContainerSpec.
      self._FillInExecutableFromApp(wrapper, app)

    if not service_bundle_name:
      raise ValueError('Service bundles must declare at least one app')

    self._FillInServiceNamesFromAnnotations(
        wrapper,
        _GetValueOfType(appc_contents, KEY_ANNOTATIONS_LIST, list,
                        'list of all annotations'))
    wrapper.SetName(container_spec_name)
    return wrapper.container_spec

  def WriteContainerSpec(self, appc_pod_manifest_path, output_path):
    """Write a ContainerSpec corresponding to |appc_pod_manifest_path| to disk.

    Args:
      appc_pod_manifest_path: path to an appc pod manifest file.
      output_path: path to file to write serialized ContainerSpec. The
          containing directory must exist, but the file may not.  This is not
          checked atomically.
    """
    if os.path.isfile(output_path):
      raise ValueError(
          'Refusing to write ContainerSpec to file %s which already exists!' %
          output_path)

    appc_contents = json.loads(osutils.ReadFile(appc_pod_manifest_path))
    # Use the file name without extension as the the name of the container spec.
    container_name = os.path.basename(appc_pod_manifest_path).rsplit('.', 1)[0]
    spec = self.GetContainerSpec(appc_contents, container_name)
    osutils.WriteFile(output_path, spec.SerializeToString())
