#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constrained Network Server. Serves files with supplied network constraints.

The CNS exposes a web based API allowing network constraints to be imposed on
file serving.

TODO(dalecurtis): Add some more docs here.

"""

import optparse
import os
import signal
import sys
import threading
import time

try:
  import cherrypy
except ImportError:
  print ('CNS requires CherryPy v3 or higher to be installed. Please install\n'
         'and try again. On Linux: sudo apt-get install python-cherrypy3\n')
  sys.exit(1)


# Default port to serve the CNS on.
_DEFAULT_SERVING_PORT = 9000

# Default port range for constrained use.
_DEFAULT_CNS_PORT_RANGE = (50000, 51000)

# Default number of seconds before a port can be torn down.
_DEFAULT_PORT_EXPIRY_TIME_SECS = 5 * 60


class PortAllocator(object):
  """Dynamically allocates/deallocates ports with a given set of constraints."""

  def __init__(self, port_range, expiry_time_secs=5 * 60):
    """Sets up initial state for the Port Allocator.

    Args:
      port_range: Range of ports available for allocation.
      expiry_time_secs: Amount of time in seconds before constrained ports are
          cleaned up.
    """
    self._port_range = port_range
    self._expiry_time_secs = expiry_time_secs

    # Keeps track of ports we've used, the creation key, and the last request
    # time for the port so they can be cached and cleaned up later.
    self._ports = {}

    # Locks port creation and cleanup. TODO(dalecurtis): If performance becomes
    # an issue a per-port based lock system can be used instead.
    self._port_lock = threading.Lock()

  def Get(self, key, **kwargs):
    """Sets up a constrained port using the requested parameters.

    Requests for the same key and constraints will result in a cached port being
    returned if possible.

    Args:
      key: Used to cache ports with the given constraints.
      **kwargs: Constraints to pass into traffic control.

    Returns:
      None if no port can be setup or the port number of the constrained port.
    """
    with self._port_lock:
      # Check port key cache to see if this port is already setup. Update the
      # cache time and return the port if so. Performance isn't a concern here,
      # so just iterate over ports dict for simplicity.
      full_key = (key,) + tuple(kwargs.values())
      for port, status in self._ports.iteritems():
        if full_key == status['key']:
          self._ports[port]['last_update'] = time.time()
          return port

      # Cleanup ports on new port requests. Do it after the cache check though
      # so we don't erase and then setup the same port.
      self._CleanupLocked(all_ports=False)

      # Performance isn't really an issue here, so just iterate over the port
      # range to find an unused port. If no port is found, None is returned.
      for port in xrange(self._port_range[0], self._port_range[1]):
        if port in self._ports:
          continue

        # TODO(dalecurtis): Integrate with shadi's scripts.
        # We've found an open port so call the script and set it up.
        #Port.Setup(port=port, **kwargs)

        self._ports[port] = {'last_update': time.time(), 'key': full_key}
        return port

  def _CleanupLocked(self, all_ports):
    """Internal cleanup method, expects lock to have already been acquired.

    See Cleanup() for more information.

    Args:
      all_ports: Should all ports be torn down regardless of expiration?
    """
    now = time.time()
    # Use .items() instead of .iteritems() so we can delete keys w/o error.
    for port, status in self._ports.items():
      expired = now - status['last_update'] > self._expiry_time_secs
      if all_ports or expired:
        cherrypy.log('Cleaning up port %d' % port)

        # TODO(dalecurtis): Integrate with shadi's scripts.
        #Port.Delete(port=port)

        del self._ports[port]

  def Cleanup(self, all_ports=False):
    """Cleans up expired ports, or if all_ports=True, all allocated ports.

    By default, ports which haven't been used for self._expiry_time_secs are
    torn down. If all_ports=True then they are torn down regardless.

    Args:
      all_ports: Should all ports be torn down regardless of expiration?
    """
    with self._port_lock:
      self._CleanupLocked(all_ports)


class ConstrainedNetworkServer(object):
  """A CherryPy-based HTTP server for serving files with network constraints."""

  def __init__(self, options, port_allocator):
    """Sets up initial state for the CNS.

    Args:
      options: optparse based class returned by ParseArgs()
      port_allocator: A port allocator instance.
    """
    self._options = options
    self._port_allocator = port_allocator

  @cherrypy.expose
  def ServeConstrained(self, f=None, bandwidth=None, latency=None, loss=None):
    """Serves the requested file with the requested constraints.

    Subsequent requests for the same constraints from the same IP will share the
    previously created port. If no constraints are provided the file is served
    as is.

    Args:
      f: path relative to http root of file to serve.
      bandwidth: maximum allowed bandwidth for the provided port (integer
          in kbit/s).
      latency: time to add to each packet (integer in ms).
      loss: percentage of packets to drop (integer, 0-100).
    """
    # CherryPy is a bit wonky at detecting parameters, so just make them all
    # optional and validate them ourselves.
    if not f:
      raise cherrypy.HTTPError(400, 'Invalid request. File must be specified.')

    # Sanitize and check the path to prevent www-root escapes.
    sanitized_path = os.path.abspath(os.path.join(self._options.www_root, f))
    if not sanitized_path.startswith(self._options.www_root):
      raise cherrypy.HTTPError(403, 'Invalid file requested.')

    # Check existence early to prevent wasted constraint setup.
    if not os.path.exists(sanitized_path):
      raise cherrypy.HTTPError(404, 'File not found.')

    # If there are no constraints, just serve the file.
    if bandwidth is None and latency is None and loss is None:
      return cherrypy.lib.static.serve_file(sanitized_path)

    # Validate inputs. isdigit() guarantees a natural number.
    if bandwidth and not bandwidth.isdigit():
      raise cherrypy.HTTPError(400, 'Invalid bandwidth constraint.')

    if latency and not latency.isdigit():
      raise cherrypy.HTTPError(400, 'Invalid latency constraint.')

    if loss and not loss.isdigit() and not int(loss) <= 100:
      raise cherrypy.HTTPError(400, 'Invalid loss constraint.')

    # Allocate a port using the given constraints. If a port with the requested
    # key is already allocated, it will be reused.
    #
    # TODO(dalecurtis): The key cherrypy.request.remote.ip might not be unique
    # if build slaves are sharing the same VM.
    constrained_port = self._port_allocator.Get(
        cherrypy.request.remote.ip, bandwidth=bandwidth, latency=latency,
        loss=loss)

    if not constrained_port:
      raise cherrypy.HTTPError(503, 'Service unavailable. Out of ports.')

    # Build constrained URL. Only pass on the file parameter.
    constrained_url = '%s?file=%s' % (
        cherrypy.url().replace(
            ':%d' % self._options.port, ':%d' % constrained_port),
        f)

    # Redirect request to the constrained port.
    cherrypy.lib.cptools.redirect(constrained_url, internal=False)


def ParseArgs():
  """Define and parse the command-line arguments."""
  parser = optparse.OptionParser()

  parser.add_option('--expiry-time', type='int',
                    default=_DEFAULT_PORT_EXPIRY_TIME_SECS,
                    help=('Number of seconds before constrained ports expire '
                          'and are cleaned up. Default: %default'))
  parser.add_option('--port', type='int', default=_DEFAULT_SERVING_PORT,
                    help='Port to serve the API on. Default: %default')
  parser.add_option('--port-range', default=_DEFAULT_CNS_PORT_RANGE,
                    help=('Range of ports for constrained serving. Specify as '
                          'a comma separated value pair. Default: %default'))
  parser.add_option('--interface', default='eth0',
                    help=('Interface to setup constraints on. Use lo for a '
                          'local client. Default: %default'))
  parser.add_option('--threads', type='int',
                    default=cherrypy._cpserver.Server.thread_pool,
                    help=('Number of threads in the thread pool. Default: '
                          '%default'))
  parser.add_option('--www-root', default=os.getcwd(),
                    help=('Directory root to serve files from. Defaults to the '
                          'current directory: %default'))

  options = parser.parse_args()[0]

  # Convert port range into the desired tuple format.
  try:
    if isinstance(options.port_range, str):
      options.port_range = [int(port) for port in options.port_range.split(',')]
  except ValueError:
    parser.error('Invalid port range specified.')

  # Normalize the path to remove any . or ..
  options.www_root = os.path.normpath(options.www_root)

  return options


def Main():
  """Configure and start the ConstrainedNetworkServer."""
  options = ParseArgs()

  cherrypy.config.update(
      {'server.socket_host': '::', 'server.socket_port': options.port})

  if options.threads:
    cherrypy.config.update({'server.thread_pool': options.threads})

  # Setup port allocator here so we can call cleanup on failures/exit.
  pa = PortAllocator(options.port_range, expiry_time_secs=options.expiry_time)

  try:
    cherrypy.quickstart(ConstrainedNetworkServer(options, pa))
  finally:
    # Disable Ctrl-C handler to prevent interruption of cleanup.
    signal.signal(signal.SIGINT, lambda signal, frame: None)
    pa.Cleanup(all_ports=True)


if __name__ == '__main__':
  Main()
