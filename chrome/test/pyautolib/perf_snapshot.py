#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance snapshot utility for pyauto tests.

Wrapper around Chrome DevTools (mimics the front-end) to collect profiling info
associated with a Chrome tab.  This script collects snapshots of the v8
(Javascript engine) heap associated with the Chrome tab.

Assumption: there must already be a running Chrome instance on the local machine
that has been passed flag '--remote-debugging-port=9222' to enable remote
debugging on port 9222.

This script can be run directly from the command line.  Run with '-h' to see
the accepted options.  Option -i allows the script to be run in interactive
mode, which continually prompts the user to take a snapshot or quit the program.
This is useful for on-demand snapshotting while the user manually interacts with
a running browser.

To take v8 heap snapshots from another automated script (e.g., from pyauto):

import perf_snapshot
snapshotter = perf_snapshot.PerformanceSnapshotter()
snapshot_info = snapshotter.HeapSnapshot()
new_snapshot_info = snapshotter.HeapSnapshot()

HeapSnapshot() returns a list of dictionaries, where each dictionary contains
the summarized info for a single v8 heap snapshot.  See the initialization
parameters for class PerformanceSnapshotter to control how snapshots are taken.
"""

import asyncore
import datetime
import logging
import optparse
import simplejson
import socket
import threading
import time
import urllib2
import urlparse


class _V8HeapSnapshotParser(object):
  """Parses v8 heap snapshot data.

  Public Methods:
    ParseSnapshotData: A static method that parses v8 heap snapshot data and
                       returns a dictionary of the summarized results.
  """
  _CHILD_TYPES = ['context', 'element', 'property', 'internal', 'hidden',
                  'shortcut']
  _NODE_TYPES = ['hidden', 'array', 'string', 'object', 'code', 'closure',
                 'regexp', 'number', 'native']

  @staticmethod
  def ParseSnapshotData(raw_data):
    """Parses raw v8 heap snapshot data and returns the summarized results.

    The raw heap snapshot data is represented as a JSON object with the
    following keys: 'snapshot', 'nodes', and 'strings'.

    The 'snapshot' value provides the 'title' and 'uid' attributes for the
    snapshot.  For example:
    { u'title': u'org.webkit.profiles.user-initiated.1', u'uid': 1}

    The 'nodes' value is a list of node information from the v8 heap, with a
    special first element that describes the node serialization layout (see
    HeapSnapshotJSONSerializer::SerializeNodes).  All other list elements
    contain information about nodes in the v8 heap, according to the
    serialization layout.

    The 'strings' value is a list of strings, indexed by values in the 'nodes'
    list to associate nodes with strings.

    Args:
      raw_data: A string representing the raw v8 heap snapshot data.

    Returns:
      A dictionary containing the summarized v8 heap snapshot data:
      {
        'total_node_count': integer,  # Total number of nodes in the v8 heap.
        'total_shallow_size': integer, # Total heap size, in bytes.
      }
    """
    total_node_count = 0
    total_shallow_size = 0
    constructors = {}

    # TODO(dennisjeffrey): The following line might be slow, especially on
    # ChromeOS.  Investigate faster alternatives.
    heap = simplejson.loads(raw_data)

    index = 1  # Bypass the special first node list item.
    node_list = heap['nodes']
    while index < len(node_list):
      node_type = node_list[index]
      node_name = node_list[index + 1]
      node_id = node_list[index + 2]
      node_self_size = node_list[index + 3]
      node_retained_size = node_list[index + 4]
      node_dominator = node_list[index + 5]
      node_children_count = node_list[index + 6]
      index += 7

      node_children = []
      for i in range(node_children_count):
        child_type = node_list[index]
        child_type_string = _V8HeapSnapshotParser._CHILD_TYPES[int(child_type)]
        child_name_index = node_list[index + 1]
        child_to_node = node_list[index + 2]
        index += 3

        child_info = {
          'type': child_type_string,
          'name_or_index': child_name_index,
          'to_node': child_to_node,
        }
        node_children.append(child_info)

      # Get the constructor string for this node so nodes can be grouped by
      # constructor.
      # See HeapSnapshot.js: WebInspector.HeapSnapshotNode.prototype.
      type_string = _V8HeapSnapshotParser._NODE_TYPES[int(node_type)]
      constructor_name = None
      if type_string == 'hidden':
        constructor_name = '(system)'
      elif type_string == 'object':
        constructor_name = heap['strings'][int(node_name)]
      elif type_string == 'native':
        pos = heap['strings'][int(node_name)].find('/')
        if pos >= 0:
          constructor_name = heap['strings'][int(node_name)][:pos].rstrip()
        else:
          constructor_name = heap['strings'][int(node_name)]
      elif type_string == 'code':
        constructor_name = '(compiled code)'
      else:
        constructor_name = '(' + type_string + ')'

      node_obj = {
        'type': type_string,
        'name': heap['strings'][int(node_name)],
        'id': node_id,
        'self_size': node_self_size,
        'retained_size': node_retained_size,
        'dominator': node_dominator,
        'children_count': node_children_count,
        'children': node_children,
      }

      if constructor_name not in constructors:
        constructors[constructor_name] = []
      constructors[constructor_name].append(node_obj)

      total_node_count += 1
      total_shallow_size += node_self_size

    # TODO(dennisjeffrey): Have this function also return more detailed v8
    # heap snapshot data when a need for it arises (e.g., using |constructors|).
    result = {}
    result['total_node_count'] = total_node_count
    result['total_shallow_size'] = total_shallow_size
    return result


class _DevToolsSocketRequest(object):
  """A representation of a single DevToolsSocket request.

  A DevToolsSocket request is used for communication with a remote Chrome
  instance when interacting with the renderer process of a given webpage.
  Requests and results are passed as specially-formatted JSON messages,
  according to a communication protocol defined in WebKit.  The string
  representation of this request will be a JSON message that is properly
  formatted according to the communication protocol.

  Public Attributes:
    method: The string method name associated with this request.
    id: A unique integer id associated with this request.
    params: A dictionary of input parameters associated with this request.
    results: A dictionary of relevant results obtained from the remote Chrome
             instance that are associated with this request.
    is_complete: A boolean indicating whether or not this request has been sent
                 and all relevant results for it have been obtained (i.e., this
                 value is True only if all results for this request are known).
  """
  def __init__(self, method, message_id):
    """Initializes a DevToolsSocket request.

    Args:
      method: The string method name for this request.
      message_id: An integer id for this request, which is assumed to be unique
                  from among all requests.
    """
    self.method = method
    self.id = message_id
    self.params = {}
    self.results = {}
    self.is_complete = False

  def __repr__(self):
    json_dict = {}
    json_dict['method'] = self.method
    json_dict['id'] = self.id
    if self.params:
      json_dict['params'] = self.params
    return simplejson.dumps(json_dict, separators=(',', ':'))


class _DevToolsSocketClient(asyncore.dispatcher):
  """Client that communicates with a remote Chrome instance via sockets.

  This class works in conjunction with the _PerformanceSnapshotterThread class
  to communicate with a remote Chrome instance following the remote debugging
  communication protocol in WebKit.  This class performs the lower-level work
  of socket communication.

  Public Methods:
    SendMessage: Causes a specified message to be sent to the remote Chrome
                 instance.

  Public Attributes:
    handshake_done: A boolean indicating whether or not the client has completed
                    the required protocol handshake with the remote Chrome
                    instance.
    snapshotter: An instance of the _PerformanceSnapshotterThread class that is
                 working together with this class to communicate with a remote
                 Chrome instance.
  """
  def __init__(self, verbose, show_socket_messages, hostname, port, path):
    """Initializes the DevToolsSocketClient.

    Args:
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
                            socket messages sent/received when communicating
                            with the remote Chrome instance.
      hostname: The string hostname of the DevToolsSocket to which to connect.
      port: The integer port number of the DevToolsSocket to which to connect.
      path: The string path of the DevToolsSocket to which to connect.
    """
    asyncore.dispatcher.__init__(self)

    self._logger = logging.getLogger('_DevToolsSocketClient')
    self._logger.setLevel([logging.WARNING, logging.DEBUG][verbose])

    self._show_socket_messages = show_socket_messages

    self._read_buffer = ''
    self._write_buffer = ''

    self.handshake_done = False
    self.snapshotter = None

    # Connect to the remote Chrome instance and initiate the protocol handshake.
    self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
    self.connect((hostname, port))

    fields = [
      'Upgrade: WebSocket',
      'Connection: Upgrade',
      'Host: %s:%d' % (hostname, port),
      'Origin: http://%s:%d' % (hostname, port),
      'Sec-WebSocket-Key1: 4k0L66E ZU 8  5  <18 <TK 7   7',
      'Sec-WebSocket-Key2: s2  20 `# 4|  3 9   U_ 1299',
    ]
    handshake_msg = ('GET %s HTTP/1.1\r\n%s\r\n\r\n\x47\x30\x22\x2D\x5A\x3F'
                     '\x47\x58' % (path, '\r\n'.join(fields)))
    self._Write(handshake_msg.encode('utf-8'))

  def SendMessage(self, msg):
    """Causes a request message to be sent to the remote Chrome instance.

    Args:
      msg: A string message to be sent; assumed to be a JSON message in proper
           format according to the remote debugging protocol in WebKit.
    """
    # According to the communication protocol, each request message sent over
    # the wire must begin with '\x00' and end with '\xff'.
    self._Write('\x00' + msg.encode('utf-8') + '\xff')

  def _Write(self, msg):
    """Causes a raw message to be sent to the remote Chrome instance.

    Args:
      msg: A raw string message to be sent.
    """
    self._write_buffer += msg
    self.handle_write()

  def handle_write(self):
    """Called if a writable socket can be written; overridden from asyncore."""
    if self._write_buffer:
      sent = self.send(self._write_buffer)
      if self._show_socket_messages:
        msg_type = ['Handshake', 'Message'][self._write_buffer[0] == '\x00' and
                                            self._write_buffer[-1] == '\xff']
        msg = ('========================\n'
               'Sent %s:\n'
               '========================\n'
               '%s\n'
               '========================') % (msg_type,
                                              self._write_buffer[:sent-1])
        print msg
      self._write_buffer = self._write_buffer[sent:]

  def handle_read(self):
    """Called when a socket can be read; overridden from asyncore."""
    if self.handshake_done:
      # Process a message reply from the remote Chrome instance.
      self._read_buffer += self.recv(4096)
      pos = self._read_buffer.find('\xff')
      while pos >= 0:
        pos += len('\xff')
        data = self._read_buffer[:pos-len('\xff')]
        pos2 = data.find('\x00')
        if pos2 >= 0:
          data = data[pos2 + 1:]
        self._read_buffer = self._read_buffer[pos:]
        if self._show_socket_messages:
          msg = ('========================\n'
                 'Received Message:\n'
                 '========================\n'
                 '%s\n'
                 '========================') % data
          print msg
        if self.snapshotter:
          self.snapshotter.NotifyReply(data)
        pos = self._read_buffer.find('\xff')
    else:
      # Process a handshake reply from the remote Chrome instance.
      self._read_buffer += self.recv(4096)
      pos = self._read_buffer.find('\r\n\r\n')
      if pos >= 0:
        pos += len('\r\n\r\n')
        data = self._read_buffer[:pos]
        self._read_buffer = self._read_buffer[pos:]
        self.handshake_done = True
        if self._show_socket_messages:
          msg = ('=========================\n'
                 'Received Handshake Reply:\n'
                 '=========================\n'
                 '%s\n'
                 '=========================') % data
          print msg

  def handle_close(self):
    """Called when the socket is closed; overridden from asyncore."""
    self.close()

  def writable(self):
    """Determines if writes can occur for this socket; overridden from asyncore.

    Returns:
      True, if there is something to write to the socket, or
      False, otherwise.
    """
    return len(self._write_buffer) > 0

  def handle_expt(self):
    """Called when out-of-band data exists; overridden from asyncore."""
    self.handle_error()

  def handle_error(self):
    """Called when an exception is raised; overridden from asyncore."""
    self.close()
    self.snapshotter.NotifySocketClientException()
    asyncore.dispatcher.handle_error(self)


class _PerformanceSnapshotterThread(threading.Thread):
  """Manages communication with a remote Chrome instance to take snapshots.

  This class works in conjunction with the _DevToolsSocketClient class to
  communicate with a remote Chrome instance following the remote debugging
  communication protocol in WebKit.  This class performs the higher-level work
  of managing request and reply messages, whereas _DevToolsSocketClient handles
  the lower-level work of socket communication.

  Public Methods:
    NotifyReply: Notifies the current object of a reply message that has been
                 received from the remote Chrome instance (which would have been
                 sent in response to an earlier request).  Called by the
                 _DevToolsSocketClient.
    NotifySocketClientException: Notifies the current object that the
                                 _DevToolsSocketClient encountered an exception.
                                 Called by the _DevToolsSocketClient.
    run: Starts the thread of execution for this object.  Invoked implicitly
         by calling the start() method on this object.

  Public Attributes:
    collected_heap_snapshot_data: A list of dictionaries, where each dictionary
                                  contains the information for a taken snapshot.
  """
  _HEAP_SNAPSHOT_MESSAGES = [
    'Page.getResourceTree',
    'Debugger.enable',
    'Profiler.clearProfiles',
    'Profiler.takeHeapSnapshot',
    'Profiler.getProfile',
  ]

  def __init__(
      self, tab_index, output_file, interval, num_snapshots, verbose,
      show_socket_messages, interactive_mode):
    """Initializes a _PerformanceSnapshotterThread object.

    Args:
      tab_index: The integer index of the tab in the remote Chrome instance to
                 use for snapshotting.
      output_file: The string name of an output file in which to write results.
                   Use None to refrain from writing results to an output file.
      interval: An integer number of seconds to wait after a snapshot is
                completed, before starting the next snapshot.
      num_snapshots: An integer number of snapshots to take before terminating.
                     Use 0 to take snapshots indefinitely (you must manually
                     kill the script in this case).
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
                            socket messages sent/received when communicating
                            with the remote Chrome instance.
      interactive_mode: A boolean indicating whether or not to take snapshots
                        in interactive mode.

    Raises:
      RuntimeError: When no proper connection can be made to a remote Chrome
                    instance.
    """
    threading.Thread.__init__(self)

    self._logger = logging.getLogger('_PerformanceSnapshotterThread')
    self._logger.setLevel([logging.WARNING, logging.DEBUG][verbose])

    self._output_file = output_file
    self._interval = interval
    self._num_snapshots = num_snapshots
    self._interactive_mode = interactive_mode

    self._next_request_id = 1
    self._requests = []
    self._current_heap_snapshot = []
    self._url = ''
    self.collected_heap_snapshot_data = []
    self.last_snapshot_start_time = 0

    self._killed = False

    # Create a DevToolsSocket client and wait for it to complete the remote
    # debugging protocol handshake with the remote Chrome instance.
    result = self._IdentifyDevToolsSocketConnectionInfo(tab_index)
    self._client = _DevToolsSocketClient(
        verbose, show_socket_messages, result['host'], result['port'],
        result['path'])
    self._client.snapshotter = self
    while asyncore.socket_map:
      if self._client.handshake_done or self._killed:
        break
      asyncore.loop(timeout=1, count=1)

  def NotifyReply(self, msg):
    """Notifies of a reply message received from the remote Chrome instance.

    Args:
      msg: A string reply message received from the remote Chrome instance;
           assumed to be a JSON message formatted according to the remote
           debugging communication protocol in WebKit.
    """
    reply_dict = simplejson.loads(msg)
    if 'result' in reply_dict:
      # This is the result message associated with a previously-sent request.
      request = self._GetRequestWithId(reply_dict['id'])
      if request:
        request.is_complete = True
      if 'frameTree' in reply_dict['result']:
        self._url = reply_dict['result']['frameTree']['frame']['url']
    elif 'method' in reply_dict:
      # This is an auxiliary message sent from the remote Chrome instance.
      if reply_dict['method'] == 'Profiler.addProfileHeader':
        snapshot_req = self._GetFirstIncompleteRequest(
            'Profiler.takeHeapSnapshot')
        if snapshot_req:
          snapshot_req.results['uid'] = reply_dict['params']['header']['uid']
      elif reply_dict['method'] == 'Profiler.addHeapSnapshotChunk':
        self._current_heap_snapshot.append(reply_dict['params']['chunk'])
      elif reply_dict['method'] == 'Profiler.finishHeapSnapshot':
        # A heap snapshot has been completed.  Analyze and output the data.
        self._logger.debug('Heap snapshot taken: %s', self._url)
        self._logger.debug('Time to request snapshot raw data: %.2f sec',
                           time.time() - self.last_snapshot_start_time)
        # TODO(dennisjeffrey): Parse the heap snapshot on-the-fly as the data
        # is coming in over the wire, so we can avoid storing the entire
        # snapshot string in memory.
        self._logger.debug('Now analyzing heap snapshot...')
        parser = _V8HeapSnapshotParser()
        time_start = time.time()
        raw_snapshot_data = ''.join(self._current_heap_snapshot)
        self._logger.debug('Raw snapshot data size: %.2f MB',
                           len(raw_snapshot_data) / (1024.0 * 1024.0))
        result = parser.ParseSnapshotData(raw_snapshot_data)
        self._logger.debug('Time to parse data: %.2f sec',
                           time.time() - time_start)
        num_nodes = result['total_node_count']
        total_size = result['total_shallow_size']
        total_size_str = self._ConvertBytesToHumanReadableString(total_size)
        timestamp = time.time()

        self.collected_heap_snapshot_data.append(
            {'url': self._url,
             'timestamp': timestamp,
             'total_node_count': num_nodes,
             'total_heap_size': total_size,})

        if self._output_file:
          f = open(self._output_file, 'a')
          f.write('\n')
          now = datetime.datetime.now()
          f.write('[%s]\nSnapshot for: %s\n' %
                  (now.strftime('%Y-%B-%d %I:%M:%S %p'), self._url))
          f.write('  Total node count: %d\n' % num_nodes)
          f.write('  Total shallow size: %s\n' % total_size_str)
          f.close()
        self._logger.debug('Heap snapshot analysis complete (%s).',
                           total_size_str)

  def NotifySocketClientException(self):
    """Notifies that the _DevToolsSocketClient encountered an exception."""
    self._killed = True

  @staticmethod
  def _ConvertBytesToHumanReadableString(num_bytes):
    """Converts an integer number of bytes into a human-readable string.

    Args:
      num_bytes: An integer number of bytes.

    Returns:
      A human-readable string representation of the given number of bytes.
    """
    if num_bytes < 1024:
      return '%d B' % num_bytes
    elif num_bytes < 1048576:
      return '%.2f KB' % (num_bytes / 1024.0)
    else:
      return '%.2f MB' % (num_bytes / 1048576.0)

  def _IdentifyDevToolsSocketConnectionInfo(self, tab_index):
    """Identifies DevToolsSocket connection info from a remote Chrome instance.

    Args:
      tab_index: The integer index of the tab in the remote Chrome instance to
                 which to connect.

    Returns:
      A dictionary containing the DevToolsSocket connection info:
      {
        'host': string,
        'port': integer,
        'path': string,
      }

    Raises:
      RuntimeError: When DevToolsSocket connection info cannot be identified.
    """
    try:
      # TODO(dennisjeffrey): Do not assume port 9222.  The port should be passed
      # as input to this function.
      f = urllib2.urlopen('http://localhost:9222/json')
      result = f.read();
      result = simplejson.loads(result)
    except urllib2.URLError, e:
      raise RuntimeError(
          'Error accessing Chrome instance debugging port: ' + str(e))

    if tab_index >= len(result):
      raise RuntimeError(
          'Specified tab index %d doesn\'t exist (%d tabs found)' %
          (tab_index, len(result)))

    if 'webSocketDebuggerUrl' not in result[tab_index]:
      raise RuntimeError('No socket URL exists for the specified tab.')

    socket_url = result[tab_index]['webSocketDebuggerUrl']
    parsed = urlparse.urlparse(socket_url)
    # On ChromeOS, the "ws://" scheme may not be recognized, leading to an
    # incorrect netloc (and empty hostname and port attributes) in |parsed|.
    # Change the scheme to "http://" to fix this.
    if not parsed.hostname or not parsed.port:
      socket_url = 'http' + socket_url[socket_url.find(':'):]
      parsed = urlparse.urlparse(socket_url)
      # Warning: |parsed.scheme| is incorrect after this point.
    return ({'host': parsed.hostname,
             'port': parsed.port,
             'path': parsed.path})

  def _ResetRequests(self):
    """Clears snapshot-related info in preparation for a new snapshot."""
    self._requests = []
    self._current_heap_snapshot = []
    self._url = ''

  def _GetRequestWithId(self, request_id):
    """Identifies the request with the specified id.

    Args:
      request_id: An integer request id; should be unique for each request.

    Returns:
      A request object associated with the given id if found, or
      None otherwise.
    """
    found_request = [x for x in self._requests if x.id == request_id]
    if found_request:
      return found_request[0]
    return None

  def _GetFirstIncompleteRequest(self, method):
    """Identifies the first incomplete request with the given method name.

    Args:
      method: The string method name of the request for which to search.

    Returns:
      The first request object in the request list that is not yet complete and
      is also associated with the given method name, or
      None if no such request object can be found.
    """
    for request in self._requests:
      if not request.is_complete and request.method == method:
        return request
    return None

  def _GetLatestRequestOfType(self, ref_req, method):
    """Identifies the latest specified request before a reference request.

    This function finds the latest request with the specified method that
    occurs before the given reference request.

    Returns:
      The latest request object with the specified method, if found, or
      None otherwise.
    """
    start_looking = False
    for request in self._requests[::-1]:
      if request.id == ref_req.id:
        start_looking = True
      elif start_looking:
        if request.method == method:
          return request
    return None

  def _FillInParams(self, request):
    """Fills in parameters for requests as necessary before the request is sent.

    Args:
      request: The request object associated with a request message that is
               about to be sent.
    """
    if request.method == 'Profiler.takeHeapSnapshot':
      # We always want detailed heap snapshot information.
      request.params = {'detailed': True}
    elif request.method == 'Profiler.getProfile':
      # To actually request the snapshot data from a previously-taken snapshot,
      # we need to specify the unique uid of the snapshot we want.
      # The relevant uid should be contained in the last
      # 'Profiler.takeHeapSnapshot' request object.
      last_req = self._GetLatestRequestOfType(request,
                                              'Profiler.takeHeapSnapshot')
      if last_req and 'uid' in last_req.results:
        request.params = {'type': 'HEAP', 'uid': last_req.results['uid']}

  def _TakeHeapSnapshot(self):
    """Takes a heap snapshot by communicating with _DevToolsSocketClient.

    Returns:
      A boolean indicating whether the heap snapshot was taken successfully.
      This can be False if the current thread is killed before the snapshot
      is finished being taken.
    """
    if self._killed:
      return False
    self.last_snapshot_start_time = time.time()
    self._logger.debug('Taking heap snapshot...')
    self._ResetRequests()

    # Prepare the request list.
    for message in self._HEAP_SNAPSHOT_MESSAGES:
      self._requests.append(
          _DevToolsSocketRequest(message, self._next_request_id))
      self._next_request_id += 1

    # Send out each request.  Wait until each request is complete before sending
    # the next request.
    for request in self._requests:
      self._FillInParams(request)
      self._client.SendMessage(str(request))
      while not request.is_complete:
        if self._killed:
          return False
        time.sleep(0.1)
    return True

  def run(self):
    """Start _PerformanceSnapshotterThread; overridden from threading.Thread."""
    if self._interactive_mode:
      # Carry out interactive snapshotting.
      self._logger.debug('Entering interactive snapshotting mode.')
      while True:
        inp = raw_input('Enter "s" to take a heap snapshot, or "q" to quit> ')
        if inp.lower() in ['s']:
          if not self._TakeHeapSnapshot():
            return
        elif inp.lower() in ['q', 'quit']:
          self._client.close()
          self._logger.debug('Snapshotter thread finished.')
          return
    else:
      # Carry out automatic periodic snapshotting.
      if self._num_snapshots == 0:
        # Snapshot indefinitely until the user manually terminates the process.
        self._logger.debug('Entering forever snapshot mode.')
        while True:
          if not self._TakeHeapSnapshot():
            return
          self._logger.debug('Waiting %d seconds...', self._interval)
          time.sleep(self._interval)
      else:
        # Perform the requested number of snapshots, then terminate.
        self._logger.debug('Entering %d snapshot mode.', self._num_snapshots)
        for snapshot_num in range(self._num_snapshots):
          if not self._TakeHeapSnapshot():
            return
          self._logger.debug('Completed snapshot %d of %d.', snapshot_num + 1,
                             self._num_snapshots)
          if snapshot_num + 1 < self._num_snapshots:
            self._logger.debug('Waiting %d seconds...',  self._interval)
            time.sleep(self._interval)
        self._client.close()
        self._logger.debug('Snapshotter thread finished.')


class PerformanceSnapshotter(object):
  """Main class for taking v8 heap snapshots.

  Public Methods:
    HeapSnapshot: Begins taking heap snapshots according to the initialization
                  parameters for the current object.
    SetInteractiveMode: Sets the current object to take snapshots in interactive
                        mode. Only used by the main() function in this script
                        when the 'interactive mode' command-line flag is set.
  """
  DEFAULT_SNAPSHOT_INTERVAL = 30

  # TODO(dennisjeffrey): Allow a user to specify a window index too (not just a
  # tab index), when running through PyAuto.
  def __init__(
      self, tab_index=0, output_file=None, interval=DEFAULT_SNAPSHOT_INTERVAL,
      num_snapshots=1, verbose=False, show_socket_messages=False):
    """Initializes a PerformanceSnapshotter object.

    Args:
      tab_index: The integer index of the tab in the remote Chrome instance to
                 use for snapshotting.  Defaults to 0 (the first tab).
      output_file: The string name of an output file in which to write results.
                   Use None to refrain from writing results to an output file.
      interval: An integer number of seconds to wait after a snapshot is
                completed, before starting the next snapshot.
      num_snapshots: An integer number of snapshots to take before terminating.
                     Use 0 to take snapshots indefinitely (you must manually
                     kill the script in this case).
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
                            socket messages sent/received when communicating
                            with the remote Chrome instance.
    """
    self._tab_index = tab_index
    self._output_file = output_file
    self._interval = interval
    self._num_snapshots = num_snapshots
    self._verbose = verbose
    self._show_socket_messages = show_socket_messages

    logging.basicConfig()
    self._logger = logging.getLogger('PerformanceSnapshotter')
    self._logger.setLevel([logging.WARNING, logging.DEBUG][verbose])

    self._interactive_mode = False

  def HeapSnapshot(self):
    """Takes v8 heap snapshots until done, according to initialization params.

    Returns:
      A list of dictionaries, where each dictionary contains the summarized
      information for a single v8 heap snapshot that was taken:
      {
        'url': string,  # URL of the webpage that was snapshotted.
        'timestamp': float,  # Time when snapshot taken (seconds since epoch).
        'total_node_count': integer,  # Total number of nodes in the v8 heap.
        'total_heap_size': integer,  # Total heap size (number of bytes).
      }
    """
    snapshotter_thread = _PerformanceSnapshotterThread(
        self._tab_index, self._output_file, self._interval, self._num_snapshots,
        self._verbose, self._show_socket_messages, self._interactive_mode)
    snapshotter_thread.start()
    try:
      while asyncore.socket_map:
        if not snapshotter_thread.is_alive():
          break
        asyncore.loop(timeout=1, count=1)
    except KeyboardInterrupt:
      pass
    self._logger.debug('Waiting for snapshotter thread to die...')
    snapshotter_thread.join()
    self._logger.debug('Done taking snapshots.')
    return snapshotter_thread.collected_heap_snapshot_data

  def SetInteractiveMode(self):
    """Sets the current object to take snapshots in interactive mode."""
    self._interactive_mode = True


def main():
  """Main function to enable running this script from the command line."""
  # Process command-line arguments.
  parser = optparse.OptionParser()
  parser.add_option(
      '-t', '--tab-index', dest='tab_index', type='int', default=0,
      help='Index of the tab to snapshot. Defaults to 0 (first tab).')
  parser.add_option(
      '-f', '--file', dest='output_file',
      help='Output file name (will append). Defaults to None, meaning that no '
           'file output is written.')
  parser.add_option(
      '-g', '--gap-time', dest='interval', type='int',
      default=PerformanceSnapshotter.DEFAULT_SNAPSHOT_INTERVAL,
      help='Gap of time to wait in-between snapshots (in seconds). Defaults '
           'to %d seconds.' % PerformanceSnapshotter.DEFAULT_SNAPSHOT_INTERVAL)
  parser.add_option(
      '-n', '--num-snapshots', dest='num_snapshots', type='int', default=0,
      help='Number of snapshots to take before terminating. Defaults to 0 '
           '(infinite): in this case, you must manually terminate the program.')
  parser.add_option(
      '-i', '--interactive-mode', dest='interactive_mode', default=False,
      action='store_true',
      help='Run in interactive mode. This mode prompts the user for input. '
           'Enter "s" to take a snapshot. Enter "q" to end the program. When '
           'run in this mode, the "-g" and "-n" flags are ignored.')
  parser.add_option(
      '-s', '--show-socket-messages', dest='show_socket_messages',
      default=False, action='store_true',
      help='Show the socket messages sent/received when communicating with '
           'the remote Chrome instance.')
  parser.add_option(
      '-v', '--verbose', dest='verbose', default=False, action='store_true',
      help='Use verbose logging.')

  options, args = parser.parse_args()

  # Begin taking v8 heap snapshots.
  snapshotter = PerformanceSnapshotter(
      tab_index=options.tab_index, output_file=options.output_file,
      interval=options.interval, num_snapshots=options.num_snapshots,
      verbose=options.verbose,
      show_socket_messages=options.show_socket_messages)

  if options.interactive_mode:
    snapshotter.SetInteractiveMode()

  snapshotter.HeapSnapshot()
  return 0


if __name__ == '__main__':
  sys.exit(main())
