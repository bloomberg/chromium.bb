#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome remote inspector utility for pyauto tests.

This script provides a python interface that acts as a front-end for Chrome's
remote inspector module, communicating via sockets to interact with Chrome in
the same way that the Developer Tools does.  This -- in theory -- should allow
a pyauto test to do anything that Chrome's Developer Tools does, as long as the
appropriate communication with the remote inspector is implemented in this
script.

This script assumes that Chrome is already running on the local machine with
flag '--remote-debugging-port=9222' to enable remote debugging on port 9222.

To use this module, first create an instance of class RemoteInspectorClient;
doing this sets up a connection to Chrome's remote inspector.  Then call the
appropriate functions on that object to perform the desired actions with the
remote inspector.  When done, call Stop() on the RemoteInspectorClient object
to stop communication with the remote inspector.

For example, to take v8 heap snapshots from a pyauto test:

import remote_inspector_client
my_client = remote_inspector_client.RemoteInspectorClient()
snapshot_info = my_client.HeapSnapshot(include_summary=True)
// Do some stuff...
new_snapshot_info = my_client.HeapSnapshot(include_summary=True)
my_client.Stop()

It is expected that a test will only use one instance of RemoteInspectorClient
at a time.  If a second instance is instantiated, a RuntimeError will be raised.
RemoteInspectorClient could be made into a singleton in the future if the need
for it arises.
"""

import asyncore
import datetime
import logging
import optparse
import pprint
import re
import simplejson
import socket
import sys
import threading
import time
import urllib2
import urlparse


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
    is_fulfilled: A boolean indicating whether or not this request has been sent
        and all relevant results for it have been obtained (i.e., this value is
        True only if all results for this request are known).
    is_fulfilled_condition: A threading.Condition for waiting for the request to
        be fulfilled.
  """

  def __init__(self, method, params, message_id):
    """Initialize.

    Args:
      method: The string method name for this request.
      message_id: An integer id for this request, which is assumed to be unique
          from among all requests.
    """
    self.method = method
    self.id = message_id
    self.params = params
    self.results = {}
    self.is_fulfilled = False
    self.is_fulfilled_condition = threading.Condition()

  def __repr__(self):
    json_dict = {}
    json_dict['method'] = self.method
    json_dict['id'] = self.id
    if self.params:
      json_dict['params'] = self.params
    return simplejson.dumps(json_dict, separators=(',', ':'))


class _DevToolsSocketClient(asyncore.dispatcher):
  """Client that communicates with a remote Chrome instance via sockets.

  This class works in conjunction with the _RemoteInspectorThread class to
  communicate with a remote Chrome instance following the remote debugging
  communication protocol in WebKit.  This class performs the lower-level work
  of socket communication.

  Public Attributes:
    handshake_done: A boolean indicating whether or not the client has completed
        the required protocol handshake with the remote Chrome instance.
    inspector_thread: An instance of the _RemoteInspectorThread class that is
        working together with this class to communicate with a remote Chrome
        instance.
  """

  def __init__(self, verbose, show_socket_messages, hostname, port, path):
    """Initialize.

    Args:
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
          socket messages sent/received when communicating with the remote
          Chrome instance.
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

    self._socket_buffer_lock = threading.Lock()

    self.handshake_done = False
    self.inspector_thread = None

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
    self._socket_buffer_lock.acquire()
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
    self._socket_buffer_lock.release()

  def handle_read(self):
    """Called when a socket can be read; overridden from asyncore."""
    self._socket_buffer_lock.acquire()
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
        if self.inspector_thread:
          self.inspector_thread.NotifyReply(data)
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
    self._socket_buffer_lock.release()

  def handle_close(self):
    """Called when the socket is closed; overridden from asyncore."""
    if self._show_socket_messages:
      msg = ('=========================\n'
             'Socket closed.\n'
             '=========================')
      print msg
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
    if self._show_socket_messages:
      msg = ('=========================\n'
             'Socket error.\n'
             '=========================')
      print msg
    self.close()
    self.inspector_thread.ClientSocketExceptionOccurred()
    asyncore.dispatcher.handle_error(self)


class _RemoteInspectorThread(threading.Thread):
  """Manages communication using Chrome's remote inspector protocol.

  This class works in conjunction with the _DevToolsSocketClient class to
  communicate with a remote Chrome instance following the remote inspector
  communication protocol in WebKit.  This class performs the higher-level work
  of managing request and reply messages, whereas _DevToolsSocketClient handles
  the lower-level work of socket communication.
  """

  def __init__(self, url, tab_index, tab_filter, verbose, show_socket_messages):
    """Initialize.

    Args:
      url: The base URL to connent to.
      tab_index: The integer index of the tab in the remote Chrome instance to
          use for snapshotting.
      tab_filter: When specified, is run over tabs of the remote Chrome
          instances to choose which one to connect to.
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
          socket messages sent/received when communicating with the remote
          Chrome instance.
    """
    threading.Thread.__init__(self)
    self._logger = logging.getLogger('_RemoteInspectorThread')
    self._logger.setLevel([logging.WARNING, logging.DEBUG][verbose])

    self._killed = False
    self._requests = []
    self._action_queue = []
    self._action_queue_condition = threading.Condition()
    self._action_specific_callback = None  # Callback only for current action.
    self._action_specific_callback_lock = threading.Lock()
    self._general_callbacks = []  # General callbacks that can be long-lived.
    self._general_callbacks_lock = threading.Lock()
    self._condition_to_wait = None

    # Create a DevToolsSocket client and wait for it to complete the remote
    # debugging protocol handshake with the remote Chrome instance.
    result = self._IdentifyDevToolsSocketConnectionInfo(
        url, tab_index, tab_filter)
    self._client = _DevToolsSocketClient(
        verbose, show_socket_messages, result['host'], result['port'],
        result['path'])
    self._client.inspector_thread = self
    while asyncore.socket_map:
      if self._client.handshake_done or self._killed:
        break
      asyncore.loop(timeout=1, count=1, use_poll=True)

  def ClientSocketExceptionOccurred(self):
    """Notifies that the _DevToolsSocketClient encountered an exception."""
    self.Kill()

  def NotifyReply(self, msg):
    """Notifies of a reply message received from the remote Chrome instance.

    Args:
      msg: A string reply message received from the remote Chrome instance;
           assumed to be a JSON message formatted according to the remote
           debugging communication protocol in WebKit.
    """
    reply_dict = simplejson.loads(msg)

    # Notify callbacks of this message received from the remote inspector.
    self._action_specific_callback_lock.acquire()
    if self._action_specific_callback:
      self._action_specific_callback(reply_dict)
    self._action_specific_callback_lock.release()

    self._general_callbacks_lock.acquire()
    if self._general_callbacks:
      for callback in self._general_callbacks:
        callback(reply_dict)
    self._general_callbacks_lock.release()

    if 'result' in reply_dict:
      # This is the result message associated with a previously-sent request.
      request = self.GetRequestWithId(reply_dict['id'])
      if request:
        request.is_fulfilled_condition.acquire()
        request.is_fulfilled_condition.notify()
        request.is_fulfilled_condition.release()

  def run(self):
    """Start this thread; overridden from threading.Thread."""
    while not self._killed:
      self._action_queue_condition.acquire()
      if self._action_queue:
        # There's a request to the remote inspector that needs to be processed.
        messages, callback = self._action_queue.pop(0)
        self._action_specific_callback_lock.acquire()
        self._action_specific_callback = callback
        self._action_specific_callback_lock.release()

        # Prepare the request list.
        for message_id, message in enumerate(messages):
          self._requests.append(
              _DevToolsSocketRequest(message[0], message[1], message_id))

        # Send out each request.  Wait until each request is complete before
        # sending the next request.
        for request in self._requests:
          self._FillInParams(request)
          self._client.SendMessage(str(request))

          request.is_fulfilled_condition.acquire()
          self._condition_to_wait = request.is_fulfilled_condition
          request.is_fulfilled_condition.wait()
          request.is_fulfilled_condition.release()

          if self._killed:
            self._client.close()
            return

        # Clean up so things are ready for the next request.
        self._requests = []

        self._action_specific_callback_lock.acquire()
        self._action_specific_callback = None
        self._action_specific_callback_lock.release()

      # Wait until there is something to process.
      self._condition_to_wait = self._action_queue_condition
      self._action_queue_condition.wait()
      self._action_queue_condition.release()
    self._client.close()

  def Kill(self):
    """Notify this thread that it should stop executing."""
    self._killed = True
    # The thread might be waiting on a condition.
    if self._condition_to_wait:
      self._condition_to_wait.acquire()
      self._condition_to_wait.notify()
      self._condition_to_wait.release()

  def PerformAction(self, request_messages, reply_message_callback):
    """Notify this thread of an action to perform using the remote inspector.

    Args:
      request_messages: A list of strings representing the requests to make
          using the remote inspector.
      reply_message_callback: A callable to be invoked any time a message is
          received from the remote inspector while the current action is
          being performed.  The callable should accept a single argument,
          which is a dictionary representing a message received.
    """
    self._action_queue_condition.acquire()
    self._action_queue.append((request_messages, reply_message_callback))
    self._action_queue_condition.notify()
    self._action_queue_condition.release()

  def AddMessageCallback(self, callback):
    """Add a callback to invoke for messages received from the remote inspector.

    Args:
      callback: A callable to be invoked any time a message is received from the
          remote inspector.  The callable should accept a single argument, which
          is a dictionary representing a message received.
    """
    self._general_callbacks_lock.acquire()
    self._general_callbacks.append(callback)
    self._general_callbacks_lock.release()

  def RemoveMessageCallback(self, callback):
    """Remove a callback from the set of those to invoke for messages received.

    Args:
      callback: A callable to remove from consideration.
    """
    self._general_callbacks_lock.acquire()
    self._general_callbacks.remove(callback)
    self._general_callbacks_lock.release()

  def GetRequestWithId(self, request_id):
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

  def GetFirstUnfulfilledRequest(self, method):
    """Identifies the first unfulfilled request with the given method name.

    An unfulfilled request is one for which all relevant reply messages have
    not yet been received from the remote inspector.

    Args:
      method: The string method name of the request for which to search.

    Returns:
      The first request object in the request list that is not yet fulfilled
      and is also associated with the given method name, or
      None if no such request object can be found.
    """
    for request in self._requests:
      if not request.is_fulfilled and request.method == method:
        return request
    return None

  def _GetLatestRequestOfType(self, ref_req, method):
    """Identifies the latest specified request before a reference request.

    This function finds the latest request with the specified method that
    occurs before the given reference request.

    Args:
      ref_req: A reference request from which to start looking.
      method: The string method name of the request for which to search.

    Returns:
      The latest _DevToolsSocketRequest object with the specified method,
      if found, or None otherwise.
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
      request: The _DevToolsSocketRequest object associated with a request
               message that is about to be sent.
    """
    if request.method == 'Profiler.takeHeapSnapshot':
      # We always want detailed v8 heap snapshot information.
      request.params = {'detailed': True}
    elif request.method == 'Profiler.getHeapSnapshot':
      # To actually request the snapshot data from a previously-taken snapshot,
      # we need to specify the unique uid of the snapshot we want.
      # The relevant uid should be contained in the last
      # 'Profiler.takeHeapSnapshot' request object.
      last_req = self._GetLatestRequestOfType(request,
                                              'Profiler.takeHeapSnapshot')
      if last_req and 'uid' in last_req.results:
        request.params = {'uid': last_req.results['uid']}
    elif request.method == 'Profiler.getProfile':
      # TODO(eustas): Remove this case after M27 is released.
      last_req = self._GetLatestRequestOfType(request,
                                              'Profiler.takeHeapSnapshot')
      if last_req and 'uid' in last_req.results:
        request.params = {'type': 'HEAP', 'uid': last_req.results['uid']}

  @staticmethod
  def _IdentifyDevToolsSocketConnectionInfo(url, tab_index, tab_filter):
    """Identifies DevToolsSocket connection info from a remote Chrome instance.

    Args:
      url: The base URL to connent to.
      tab_index: The integer index of the tab in the remote Chrome instance to
          which to connect.
      tab_filter: When specified, is run over tabs of the remote Chrome instance
          to choose which one to connect to.

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
      f = urllib2.urlopen(url + '/json')
      result = f.read()
      logging.debug(result)
      result = simplejson.loads(result)
    except urllib2.URLError, e:
      raise RuntimeError(
          'Error accessing Chrome instance debugging port: ' + str(e))

    if tab_filter:
      connect_to = filter(tab_filter, result)[0]
    else:
      if tab_index >= len(result):
        raise RuntimeError(
            'Specified tab index %d doesn\'t exist (%d tabs found)' %
            (tab_index, len(result)))
      connect_to = result[tab_index]

    logging.debug(simplejson.dumps(connect_to))

    if 'webSocketDebuggerUrl' not in connect_to:
      raise RuntimeError('No socket URL exists for the specified tab.')

    socket_url = connect_to['webSocketDebuggerUrl']
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


class _RemoteInspectorDriverThread(threading.Thread):
  """Drives the communication service with the remote inspector."""

  def __init__(self):
    """Initialize."""
    threading.Thread.__init__(self)

  def run(self):
    """Drives the communication service with the remote inspector."""
    try:
      while asyncore.socket_map:
        asyncore.loop(timeout=1, count=1, use_poll=True)
    except KeyboardInterrupt:
      pass


class _V8HeapSnapshotParser(object):
  """Parses v8 heap snapshot data."""
  _CHILD_TYPES = ['context', 'element', 'property', 'internal', 'hidden',
                  'shortcut', 'weak']
  _NODE_TYPES = ['hidden', 'array', 'string', 'object', 'code', 'closure',
                 'regexp', 'number', 'native', 'synthetic']

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
        'total_v8_node_count': integer,  # Total number of nodes in the v8 heap.
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
      for i in xrange(node_children_count):
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
    result['total_v8_node_count'] = total_node_count
    result['total_shallow_size'] = total_shallow_size
    return result


class NativeMemorySnapshot(object):
  """Class representing native memory snapshot captured by Chromium DevTools.

  It is just a convenient wrapper around the snapshot structure returned over
  the remote debugging protocol. The raw snapshot structure is defined in
  WebKit/Source/WebCore/inspector/Inspector.json

  Public Methods:
    GetProcessPrivateMemorySize: The process total size.
    GetUnknownSize: Size of not instrumented parts.
    GetInstrumentedObjectsCount: Number of instrumented objects traversed by
        DevTools memory instrumentation.
    GetNumberOfInstrumentedObjectsNotInHeap: Number of instrumented objects
        visited by DevTools memory instrumentation that haven't been allocated
        by tcmalloc.
    FindMemoryBlock: Given an array of memory block names return the block.
  """
  def __init__(self, root_block):
    self._root = root_block

  def GetProcessPrivateMemorySize(self):
    return self._root['size']

  def GetUnknownSize(self):
    """Size of the memory whose owner is unknown to DevTools."""
    known_size = 0
    for child in self._root['children']:
      known_size += child['size']
    return self.GetProcessPrivateMemorySize() - known_size

  def GetInstrumentedObjectsCount(self):
    """Returns number of objects visited by DevTools memory instrumentation.

    Returns:
      Number of known instrumented objects or None if it is not available.
    """
    memory_block = self.FindMemoryBlock(['ProcessPrivateMemory',
        'InstrumentedObjectsCount'])
    if not memory_block is None:
      return memory_block['size']
    return None

  def GetNumberOfInstrumentedObjectsNotInHeap(self):
    """Returns number of instrumented objects unknown to tcmalloc.

    Returns:
      Number of known instrumented objects that are not allocated by tcmalloc,
      None if it is not available.
    """
    memory_block = self.FindMemoryBlock(['ProcessPrivateMemory',
        'InstrumentedButNotAllocatedObjectsCount'])
    if not memory_block is None:
      return memory_block['size']
    return None

  def FindMemoryBlock(self, path):
    """Find memory block with given path.

    Args:
      path: Array of block names, first element is the root block
            name, last one is the name of the block to find.

    Returns:
      Memory block with given path or None.
    """
    result = None
    children = [self._root]
    for name in path:
      if not children:
        return None
      result = None
      for child in children:
        if name == child.get('name'):
          result = child
          children = child.get('children')
    return result


# TODO(dennisjeffrey): The "verbose" option used in this file should re-use
# pyauto's verbose flag.
class RemoteInspectorClient(object):
  """Main class for interacting with Chrome's remote inspector.

  Upon initialization, a socket connection to Chrome's remote inspector will
  be established.  Users of this class should call Stop() to close the
  connection when it's no longer needed.

  Public Methods:
    Stop: Close the connection to the remote inspector.  Should be called when
        a user is done using this module.
    HeapSnapshot: Takes a v8 heap snapshot and returns the summarized data.
    GetMemoryObjectCounts: Retrieves memory object count information.
    CollectGarbage: Forces a garbage collection.
    StartTimelineEventMonitoring: Starts monitoring for timeline events.
    StopTimelineEventMonitoring: Stops monitoring for timeline events.
  """

  # TODO(dennisjeffrey): Allow a user to specify a window index too (not just a
  # tab index), when running through PyAuto.
  def __init__(self, tab_index=0, tab_filter=None,
               verbose=False, show_socket_messages=False):
    """Initialize.

    Args:
      tab_index: The integer index of the tab in the remote Chrome instance to
          which to connect.  Defaults to 0 (the first tab).
      tab_filter: When specified, is run over tabs of the remote Chrome
          instance to choose which one to connect to.
      verbose: A boolean indicating whether or not to use verbose logging.
      show_socket_messages: A boolean indicating whether or not to show the
          socket messages sent/received when communicating with the remote
          Chrome instance.
    """
    self._tab_index = tab_index
    self._tab_filter = tab_filter
    self._verbose = verbose
    self._show_socket_messages = show_socket_messages

    self._timeline_started = False

    logging.basicConfig()
    self._logger = logging.getLogger('RemoteInspectorClient')
    self._logger.setLevel([logging.WARNING, logging.DEBUG][verbose])

    # Creating _RemoteInspectorThread might raise an exception. This prevents an
    # AttributeError in the destructor.
    self._remote_inspector_thread = None
    self._remote_inspector_driver_thread = None

    # TODO(dennisjeffrey): Do not assume port 9222. The port should be passed
    # as input to this function.
    url = 'http://localhost:9222'

    self._webkit_version = self._GetWebkitVersion(url)

    # Start up a thread for long-term communication with the remote inspector.
    self._remote_inspector_thread = _RemoteInspectorThread(
        url, tab_index, tab_filter, verbose, show_socket_messages)
    self._remote_inspector_thread.start()
    # At this point, a connection has already been made to the remote inspector.

    # This thread calls asyncore.loop, which activates the channel service.
    self._remote_inspector_driver_thread = _RemoteInspectorDriverThread()
    self._remote_inspector_driver_thread.start()

  def __del__(self):
    """Called on destruction of this object."""
    self.Stop()

  def Stop(self):
    """Stop/close communication with the remote inspector."""
    if self._remote_inspector_thread:
      self._remote_inspector_thread.Kill()
      self._remote_inspector_thread.join()
      self._remote_inspector_thread = None
    if self._remote_inspector_driver_thread:
      self._remote_inspector_driver_thread.join()
      self._remote_inspector_driver_thread = None

  def HeapSnapshot(self, include_summary=False):
    """Takes a v8 heap snapshot.

    Returns:
      A dictionary containing information for a single v8 heap
      snapshot that was taken.
      {
        'url': string,  # URL of the webpage that was snapshotted.
        'raw_data': string, # The raw data as JSON string.
        'total_v8_node_count': integer,  # Total number of nodes in the v8 heap.
                                         # Only if |include_summary| is True.
        'total_heap_size': integer,  # Total v8 heap size (number of bytes).
                                     # Only if |include_summary| is True.
      }
    """
    # TODO(eustas): Remove this hack after M27 is released.
    if self._IsWebkitVersionNotOlderThan(537, 27):
      get_heap_snapshot_method = 'Profiler.getHeapSnapshot'
    else:
      get_heap_snapshot_method = 'Profiler.getProfile'

    HEAP_SNAPSHOT_MESSAGES = [
      ('Page.getResourceTree', {}),
      ('Debugger.enable', {}),
      ('Profiler.clearProfiles', {}),
      ('Profiler.takeHeapSnapshot', {}),
      (get_heap_snapshot_method, {}),
    ]

    self._current_heap_snapshot = []
    self._url = ''
    self._collected_heap_snapshot_data = {}

    done_condition = threading.Condition()

    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
                     from the remote inspector.
      """
      if 'result' in reply_dict:
        # This is the result message associated with a previously-sent request.
        request = self._remote_inspector_thread.GetRequestWithId(
            reply_dict['id'])
        if 'frameTree' in reply_dict['result']:
          self._url = reply_dict['result']['frameTree']['frame']['url']
      elif 'method' in reply_dict:
        # This is an auxiliary message sent from the remote Chrome instance.
        if reply_dict['method'] == 'Profiler.addProfileHeader':
          snapshot_req = (
              self._remote_inspector_thread.GetFirstUnfulfilledRequest(
                  'Profiler.takeHeapSnapshot'))
          if snapshot_req:
            snapshot_req.results['uid'] = reply_dict['params']['header']['uid']
        elif reply_dict['method'] == 'Profiler.addHeapSnapshotChunk':
          self._current_heap_snapshot.append(reply_dict['params']['chunk'])
        elif reply_dict['method'] == 'Profiler.finishHeapSnapshot':
          # A heap snapshot has been completed.  Analyze and output the data.
          self._logger.debug('Heap snapshot taken: %s', self._url)
          # TODO(dennisjeffrey): Parse the heap snapshot on-the-fly as the data
          # is coming in over the wire, so we can avoid storing the entire
          # snapshot string in memory.
          raw_snapshot_data = ''.join(self._current_heap_snapshot)
          self._collected_heap_snapshot_data = {
              'url': self._url,
              'raw_data': raw_snapshot_data}
          if include_summary:
            self._logger.debug('Now analyzing heap snapshot...')
            parser = _V8HeapSnapshotParser()
            time_start = time.time()
            self._logger.debug('Raw snapshot data size: %.2f MB',
                               len(raw_snapshot_data) / (1024.0 * 1024.0))
            result = parser.ParseSnapshotData(raw_snapshot_data)
            self._logger.debug('Time to parse data: %.2f sec',
                               time.time() - time_start)
            count = result['total_v8_node_count']
            self._collected_heap_snapshot_data['total_v8_node_count'] = count
            total_size = result['total_shallow_size']
            self._collected_heap_snapshot_data['total_heap_size'] = total_size

          done_condition.acquire()
          done_condition.notify()
          done_condition.release()

    # Tell the remote inspector to take a v8 heap snapshot, then wait until
    # the snapshot information is available to return.
    self._remote_inspector_thread.PerformAction(HEAP_SNAPSHOT_MESSAGES,
                                                HandleReply)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()

    return self._collected_heap_snapshot_data

  def EvaluateJavaScript(self, expression):
    """Evaluates a JavaScript expression and returns the result.

    Sends a message containing the expression to the remote Chrome instance we
    are connected to, and evaluates it in the context of the tab we are
    connected to. Blocks until the result is available and returns it.

    Returns:
      A dictionary representing the result.
    """
    EVALUATE_MESSAGES = [
      ('Runtime.evaluate', { 'expression': expression,
                             'objectGroup': 'group',
                             'returnByValue': True }),
      ('Runtime.releaseObjectGroup', { 'objectGroup': 'group' })
    ]

    self._result = None
    done_condition = threading.Condition()

    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
                    from the remote Chrome instance.
      """
      if 'result' in reply_dict and 'result' in reply_dict['result']:
        self._result = reply_dict['result']['result']['value']

        done_condition.acquire()
        done_condition.notify()
        done_condition.release()

    # Tell the remote inspector to evaluate the given expression, then wait
    # until that information is available to return.
    self._remote_inspector_thread.PerformAction(EVALUATE_MESSAGES,
                                                HandleReply)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()

    return self._result

  def GetMemoryObjectCounts(self):
    """Retrieves memory object count information.

    Returns:
      A dictionary containing the memory object count information:
      {
        'DOMNodeCount': integer,  # Total number of DOM nodes.
        'EventListenerCount': integer,  # Total number of event listeners.
      }
    """
    MEMORY_COUNT_MESSAGES = [
      ('Memory.getDOMNodeCount', {})
    ]

    self._event_listener_count = None
    self._dom_node_count = None

    done_condition = threading.Condition()
    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
                    from the remote Chrome instance.
      """
      if 'result' in reply_dict and 'domGroups' in reply_dict['result']:
        event_listener_count = 0
        dom_node_count = 0
        dom_group_list = reply_dict['result']['domGroups']
        for dom_group in dom_group_list:
          listener_array = dom_group['listenerCount']
          for listener in listener_array:
            event_listener_count += listener['count']
          dom_node_array = dom_group['nodeCount']
          for dom_element in dom_node_array:
            dom_node_count += dom_element['count']
        self._event_listener_count = event_listener_count
        self._dom_node_count = dom_node_count

        done_condition.acquire()
        done_condition.notify()
        done_condition.release()

    # Tell the remote inspector to collect memory count info, then wait until
    # that information is available to return.
    self._remote_inspector_thread.PerformAction(MEMORY_COUNT_MESSAGES,
                                                HandleReply)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()

    return {
      'DOMNodeCount': self._dom_node_count,
      'EventListenerCount': self._event_listener_count,
    }

  def GetProcessMemoryDistribution(self):
    """Retrieves info about memory distribution between renderer components.

    Returns:
      An object representing the native memory snapshot.
    """
    MEMORY_DISTRIBUTION_MESSAGES = [
      ('Profiler.collectGarbage', {}),
      ('Memory.getProcessMemoryDistribution', {})
    ]

    reply_holder = [None]
    done_condition = threading.Condition()
    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
            from the remote Chrome instance.
      """
      request_id = reply_dict['id']
      # GC command will have id = 0, the second command id = 1
      if request_id == 0:
        logging.info('Did garbage collection')
        return
      if request_id != 1:
        raise RuntimeError('Unexpected request_id: %d' % request_id)
      reply_holder[0] = reply_dict
      done_condition.acquire()
      done_condition.notify()
      done_condition.release()

    # Tell the remote inspector to perform garbage collection and capture native
    # memory snapshot.
    self._remote_inspector_thread.PerformAction(MEMORY_DISTRIBUTION_MESSAGES,
                                                HandleReply)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()
    reply = reply_holder[0]
    if 'result' in reply:
      return NativeMemorySnapshot(reply_holder[0]['result']['distribution'])
    raise RuntimeError('Unexpected protocol error: ' +
                       reply['error']['message'])

  def CollectGarbage(self):
    """Forces a garbage collection."""
    COLLECT_GARBAGE_MESSAGES = [
      ('Profiler.collectGarbage', {})
    ]

    # Tell the remote inspector to do a garbage collect.  We can return
    # immediately, since there is no result for which to wait.
    self._remote_inspector_thread.PerformAction(COLLECT_GARBAGE_MESSAGES, None)

  def StartTimelineEventMonitoring(self, event_callback):
    """Starts timeline event monitoring.

    Args:
      event_callback: A callable to invoke whenever a timeline event is observed
          from the remote inspector.  The callable should take a single input,
          which is a dictionary containing the detailed information of a
          timeline event.
    """
    if self._timeline_started:
      self._logger.warning('Timeline monitoring already started.')
      return
    TIMELINE_MESSAGES = [
      ('Timeline.start', {})
    ]

    self._event_callback = event_callback

    done_condition = threading.Condition()
    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
                    from the remote Chrome instance.
      """
      if 'result' in reply_dict:
        done_condition.acquire()
        done_condition.notify()
        done_condition.release()
      if reply_dict.get('method') == 'Timeline.eventRecorded':
        self._event_callback(reply_dict['params']['record'])

    # Tell the remote inspector to start the timeline.
    self._timeline_callback = HandleReply
    self._remote_inspector_thread.AddMessageCallback(self._timeline_callback)
    self._remote_inspector_thread.PerformAction(TIMELINE_MESSAGES, None)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()

    self._timeline_started = True

  def StopTimelineEventMonitoring(self):
    """Stops timeline event monitoring."""
    if not self._timeline_started:
      self._logger.warning('Timeline monitoring already stopped.')
      return
    TIMELINE_MESSAGES = [
      ('Timeline.stop', {})
    ]

    done_condition = threading.Condition()
    def HandleReply(reply_dict):
      """Processes a reply message received from the remote Chrome instance.

      Args:
        reply_dict: A dictionary object representing the reply message received
                    from the remote Chrome instance.
      """
      if 'result' in reply_dict:
        done_condition.acquire()
        done_condition.notify()
        done_condition.release()

    # Tell the remote inspector to stop the timeline.
    self._remote_inspector_thread.RemoveMessageCallback(self._timeline_callback)
    self._remote_inspector_thread.PerformAction(TIMELINE_MESSAGES, HandleReply)

    done_condition.acquire()
    done_condition.wait()
    done_condition.release()

    self._timeline_started = False

  def _ConvertByteCountToHumanReadableString(self, num_bytes):
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

  @staticmethod
  def _GetWebkitVersion(endpoint):
    """Fetches Webkit version information from a remote Chrome instance.

    Args:
      endpoint: The base URL to connent to.

    Returns:
      A dictionary containing Webkit version information:
      {
        'major': integer,
        'minor': integer,
      }

    Raises:
      RuntimeError: When Webkit version info can't be fetched or parsed.
    """
    try:
      f = urllib2.urlopen(endpoint + '/json/version')
      result = f.read();
      result = simplejson.loads(result)
    except urllib2.URLError, e:
      raise RuntimeError(
          'Error accessing Chrome instance debugging port: ' + str(e))

    if 'WebKit-Version' not in result:
      raise RuntimeError('WebKit-Version is not specified.')

    parsed = re.search('^(\d+)\.(\d+)', result['WebKit-Version'])
    if parsed is None:
      raise RuntimeError('WebKit-Version cannot be parsed.')

    try:
      info = {
        'major': int(parsed.group(1)),
        'minor': int(parsed.group(2)),
      }
    except ValueError:
      raise RuntimeError('WebKit-Version cannot be parsed.')

    return info

  def _IsWebkitVersionNotOlderThan(self, major, minor):
    """Compares remote Webkit version with specified one.

    Args:
      major: Major Webkit version.
      minor: Minor Webkit version.

    Returns:
      True if remote Webkit version is same or newer than specified,
      False otherwise.

    Raises:
      RuntimeError: If remote Webkit version hasn't been fetched yet.
    """
    if not hasattr(self, '_webkit_version'):
      raise RuntimeError('WebKit version has not been fetched yet.')
    version = self._webkit_version

    if version['major'] < major:
      return False
    elif version['major'] == major and version['minor'] < minor:
      return False
    else:
      return True
